/*
  Copyright (C) 2015 Solra Bizna

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define DUMP_TRAFFIC 0

#include "startup.hh"

#include <iostream>
#include <iomanip>
#include <chrono>
#include "display.hh"
#include "modal_error.hh"
#include "mac16.hh"
#include "widgets.hh"
#include "pkdb.hh"

#ifdef __WIN32__
// nothing?
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if DUMP_TRAFFIC
static char safe_char(char c) {
  if(c < 0x20 || c > 0x7E) return '.';
  else return c;
}

static void hexdump(const char* who, const uint8_t* buf, size_t size) {
  std::cout << who << ":" << std::endl;
  for(size_t base = 0; base < size; base += 16) {
    std::cout << "  ";
    for(size_t n = base; n < base+16; ++n) {
      if(n < size) std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buf[n];
      else std::cout << "  ";
      if((n&7)==7) std::cout << "  ";
      else std::cout << " ";
    }
    std::cout << "|";
    for(size_t n = base; n < base+16; ++n) {
      if(n < size) std::cout << safe_char(buf[n]);
      else std::cout << " ";
    }
    std::cout << "|" << std::endl;
  }
}
#endif

static std::forward_list<Net::SockStream*> socks = {&server_socket};

// Wait up to 1/10 second if no data is forthcoming
static const std::chrono::steady_clock::duration MAX_TIME_TO_AWAIT_READ = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(100));

static bool try_make_connection(Display& display,
                                const Net::Address& address,
                                int number,
                                std::string& err) {
  auto str = address.ToLongString();
  display.Statusf("Attempting connection to %s (%i)...", str.c_str(), number);
  err.clear();
  Net::IOResult res = server_socket.Connect(err, address);
  if(res == Net::IOResult::WOULD_BLOCK) {
    while(Net::Select(nullptr, nullptr, nullptr, nullptr, &socks, nullptr,
                      nullptr, 3000000)
          .GetWritableSockStreams().empty())
      {}
    if(server_socket.HasError(err)) {
      std::cerr << "Unable to connect to " << address.ToLongString() << ": " << err << std::endl;
      server_socket.Close();
      return false;
    }
    return true;
  }
  else {
    if(res != Net::IOResult::OKAY)
      std::cerr << "Unable to connect to " << address.ToLongString() << ": " << err << std::endl;
    return res == Net::IOResult::OKAY;
  }
}

static bool wait_on_next_read = false;
static std::chrono::steady_clock::time_point wait_end_timepoint;
static int receive_on_server_socket(void* _display, void* buf, size_t bufsz) {
  size_t len;
  std::string err;
  Net::IOResult res;
  if(wait_on_next_read) {
    std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now();
    if(now < wait_end_timepoint) {
      size_t timeout_us = std::chrono::duration_cast<std::chrono::microseconds>(wait_end_timepoint - now).count();
      (void)Net::Select(nullptr,nullptr,nullptr,&socks,nullptr,nullptr,nullptr,
                        timeout_us);
      ((Display*)_display)->Pump();
    }
  }
  len = bufsz;
  res = server_socket.Receive(err, buf, len);
  switch(res) {
  case Net::IOResult::WOULD_BLOCK:
    if(!wait_on_next_read) {
      wait_on_next_read = true;
      wait_end_timepoint = std::chrono::steady_clock::now()
        + MAX_TIME_TO_AWAIT_READ;
    }
    else wait_on_next_read = false;
    return 0;
  case Net::IOResult::CONNECTION_CLOSED:
  case Net::IOResult::MSGSIZE:
  case Net::IOResult::ERROR: return -1;
  case Net::IOResult::OKAY: break;
  }
  wait_on_next_read = false;
  return len;
}

// TODO: We won't be sending much data, so do we need to worry about buffering?
static int send_on_server_socket(void*, const void* buf, size_t bufsz) {
#if DUMP_TRAFFIC
  hexdump("to server", reinterpret_cast<const uint8_t*>(buf), bufsz);
#endif
  do {
    std::string err;
    Net::IOResult res = server_socket.Send(err, buf, bufsz);
    switch(res) {
    case Net::IOResult::WOULD_BLOCK: break;
    case Net::IOResult::CONNECTION_CLOSED:
    case Net::IOResult::MSGSIZE:
    case Net::IOResult::ERROR: return -1;
    case Net::IOResult::OKAY: return 0;;
    }
    (void)Net::Select(nullptr,nullptr,nullptr,nullptr,&socks,nullptr,nullptr);
  } while(1);
}

static void fatal(void* d, const char* why) {
  Display& display = *(Display*)d;
  std::string bah = std::string("libtttp error: ") + why;
  std::cerr << bah << std::endl;
  do_modal_error(display, bah);
  throw quit_exception();
}

static void foul(void* d, const char* why) {
  Display& display = *(Display*)d;
  std::string bah = std::string("An error occurred and it's the server's fault: ") + why;
  std::cerr << bah << std::endl;
  do_modal_error(display, bah);
  throw quit_exception();
}

bool AttemptConnection(Display& display,
                       const std::string& canon_name,
                       std::forward_list<Net::Address> targets,
                       const std::string& username,
                       const uint8_t* password_pointer, size_t password_len,
                       bool no_crypt) {
  bool server_sent_key = false;
  server_socket.Close();
#ifdef __WIN32__
  HANDLE passhandle = nullptr;
  size_t passfile_len = 0;
#define CLOSE_AUTOPASSFILE() \
  if(passhandle != nullptr) CloseHandle(passhandle)
#define UNMAP_AUTOPASSFILE() \
  UnmapViewOfFile(const_cast<uint8_t*>(password_pointer));
#else
  int passfd = -1;
  size_t passfile_len = 0;
#define CLOSE_AUTOPASSFILE() \
  if(passfd >= 0) close(passfd);
#define UNMAP_AUTOPASSFILE() \
  munmap(const_cast<uint8_t*>(password_pointer), password_len);
#endif
  if(autopassfile) {
    assert(!no_auth);
#ifdef __WIN32__
    int tname_length = MultiByteToWideChar(CP_UTF8, 0, autopassfile, -1,
                                           NULL, 0);
    TCHAR* tname = reinterpret_cast<TCHAR*>(safe_malloc(tname_length
                                                        * sizeof(TCHAR)));
    MultiByteToWideChar(CP_UTF8, 0, autopassfile, -1, tname, tname_length);
    FILE* q = _wfopen(tname, L"rb");
    if(!q) {
      Widgets::ModalInfo(display, std::string("Unable to open the password file.\n\n")+autopassfile+": "+strerror(errno), MAC16_BLACK|(MAC16_RED<<4));
      throw quit_exception();
    }
    if(fseek(q, 0, SEEK_END)) {
      Widgets::ModalInfo(display, std::string("Unable to seek in the password file.\n\n")+autopassfile+": "+strerror(errno), MAC16_BLACK|(MAC16_RED<<4));
      fclose(q);
      throw quit_exception();
    }
    passfile_len = ftell(q);
    fclose(q);
    passhandle = OpenFileMapping(FILE_MAP_READ, FALSE, tname);
    safe_free(tname);
    if(passhandle == nullptr) {
      Widgets::ModalInfo(display, "Unable to open the password file for memory mapping.", MAC16_BLACK|(MAC16_RED<<4));
      std::cerr << "Unable to open the password file for mapping: Error code " << GetLastError() << std::endl;
      throw quit_exception();
    }
#else
    passfd = open(autopassfile, O_RDONLY|O_NOCTTY);
    if(passfd < 0) {
      Widgets::ModalInfo(display, std::string("Unable to open the password file.\n\n")+autopassfile+": "+strerror(errno), MAC16_BLACK|(MAC16_RED<<4));
      throw quit_exception();
    }
    auto sought = lseek(passfd, 0, SEEK_END);
    if(sought < 0 || lseek(passfd, 0, SEEK_SET) != 0) {
      close(passfd);
      Widgets::ModalInfo(display, std::string("Unable to seek in the password file.\n\n")+autopassfile+": "+strerror(errno), MAC16_BLACK|(MAC16_RED<<4));
      throw quit_exception();
    }
    passfile_len = sought;
#endif
  }
  server_socket = std::move(Net::SockStream()); // make sure it's clean
  bool success = false;
  int idx = 0;
  std::string err;
  for(const auto& address : targets) {
    if(try_make_connection(display, address, ++idx, err)) {
      success = true;
      break;
    }
  }
  if(!success) {
    display.Statusf("");
    CLOSE_AUTOPASSFILE();
    Widgets::ModalInfo(display, std::string("All of our attempts to connect to the server failed. The error was: ")+err, MAC16_BLACK|(MAC16_ORANGE<<4));
    return false;
  }
  display.Statusf("Performing handshake...");
  // we have a connection, do the handshake
  if(tttp) {
    tttp_client_fini(tttp);
    tttp = nullptr;
  }
  tttp = tttp_client_init(&display,
                          receive_on_server_socket,
                          send_on_server_socket,
                          nullptr, fatal, foul);
  if(queue_depth > 0)
    tttp_client_set_queue_depth(tttp, queue_depth);
  tttp_client_set_mouse_resolution(tttp,
                                   display.GetCharWidth(),
                                   display.GetCharHeight());
  uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH];
  tttp_handshake_result res;
  if(!no_auth) {
    do {
      res = tttp_client_query_server(tttp, public_key, nullptr, nullptr);
      switch(res) {
      case TTTP_HANDSHAKE_REJECTED:
      case TTTP_HANDSHAKE_ADVANCE: break;
      case TTTP_HANDSHAKE_CONTINUE:
        (void)Net::Select(nullptr,nullptr,nullptr,
                          &socks,nullptr,nullptr,nullptr);
        break;
      default:
        server_socket.Close();
        CLOSE_AUTOPASSFILE();
        display.Statusf("");
        return false;
      }
    } while(res != TTTP_HANDSHAKE_REJECTED && res != TTTP_HANDSHAKE_ADVANCE);
    server_sent_key = (res == TTTP_HANDSHAKE_ADVANCE);
    if(res == TTTP_HANDSHAKE_REJECTED) {
      if(!PKDB::GetPublicKey(canon_name, public_key)) {
        display.Statusf("");
        server_socket.Close();
        Widgets::ModalInfo(display, "The server has a hidden public key. In order to connect to this server, you must obtain the server's public key elsewhere (such as from its website, or from talking to its operator) and manually add it.\n\nThe option to add a public key for a server becomes visible on the main connection menu when the " PMOD " key is held.");
        CLOSE_AUTOPASSFILE();
        return false;
      }
      /* we have a public key on file, use it */
    }
    else if(tttp_key_is_null_public_key(public_key)) {
      display.Statusf("");
      uint8_t filed_public_key[TTTP_PUBLIC_KEY_LENGTH];
      if(PKDB::GetPublicKey(canon_name, filed_public_key)) {
        server_socket.Close();
        Widgets::ModalInfo(display, "The server is refusing to confirm its identity, even though it was perfectly happy to do so in the past. SOMEONE IS PROBABLY TRYING TO IMPERSONATE THIS SERVER!\n\nContact the server operator.\n\nYOU SHOULD NOT ATTEMPT TO CONNECT AGAIN UNTIL THIS IS RESOLVED!", MAC16_BLACK|(MAC16_RED<<4));
        CLOSE_AUTOPASSFILE();
        return false;
      }
      else if(!Widgets::ModalConfirm(display, "This server is refusing to authenticate itself. Its identity cannot be proven. Encryption is still technically possible, but there's no way to know who you're actually communicating with.\n\nAre you sure you want to continue connecting?")) {
        server_socket.Close();
        CLOSE_AUTOPASSFILE();
        return false;
      }
      display.Statusf("Performing handshake...");
    }
    else {
      uint8_t filed_public_key[TTTP_PUBLIC_KEY_LENGTH];
      if(PKDB::GetPublicKey(canon_name, filed_public_key)) {
        if(memcmp(public_key, filed_public_key, TTTP_PUBLIC_KEY_LENGTH)) {
          display.Statusf("");
          server_socket.Close();
          CLOSE_AUTOPASSFILE();
          char old_fingerprint[TTTP_FINGERPRINT_BUFFER_SIZE];
          tttp_get_key_fingerprint(public_key, old_fingerprint);
          char new_fingerprint[TTTP_FINGERPRINT_BUFFER_SIZE];
          tttp_get_key_fingerprint(filed_public_key, new_fingerprint);
          Widgets::ModalInfo(display, std::string("The server's public key has changed. Someone may be trying to impersonate this server... or the key may have changed due to unavoidable circumstances. Contact the server operator.\n\nOld fingerprint: ")+old_fingerprint+"\nNew fingerprint: "+new_fingerprint+"\n\nYOU SHOULD NOT ATTEMPT TO CONNECT AGAIN UNTIL THIS IS RESOLVED!", MAC16_BLACK|(MAC16_RED<<4));
          return false;
        }
      }
      else {
        char fingerprint[TTTP_FINGERPRINT_BUFFER_SIZE];
        tttp_get_key_fingerprint(public_key, fingerprint);
        display.Statusf("");
        if(Widgets::ModalConfirm(display, std::string("You have never connected to ")+canon_name+" before. Its public key fingerprint, which you can use to confirm the server's identity, is:\n\n"+fingerprint+"\n\nWould you like to remember this key, and continue connecting?"))
          PKDB::AddPublicKey(canon_name, public_key);
        else
          return false;
        display.Statusf("Performing handshake...");
      }
    }
  }
  tttp_client_request_flags(tttp, TTTP_FLAG_PRECISE_MOUSE |
                            (no_crypt ? 0 : TTTP_FLAG_ENCRYPTION));
  do {
    res = tttp_client_pump_flags(tttp);
    switch(res) {
    case TTTP_HANDSHAKE_ADVANCE: break;
    case TTTP_HANDSHAKE_CONTINUE:
      (void)Net::Select(nullptr,nullptr,nullptr,
                        &socks,nullptr,nullptr,nullptr);
      break;
    default:
      server_socket.Close();
      CLOSE_AUTOPASSFILE();
      display.Statusf("");
      return false;
    }
  } while(res != TTTP_HANDSHAKE_ADVANCE);
  uint32_t final_flags = tttp_client_get_flags(tttp);
  if(final_flags & TTTP_FLAG_FUTURE_CRYPTO) {
    server_socket.Close();
    CLOSE_AUTOPASSFILE();
    display.Statusf("");
    Widgets::ModalInfo(display,
                       "The server requires the \"future crypto\" extension"
                       " to the TTTP protocol. This client does not support"
                       " it.\n\nTry upgrading to a newer client.",
                       MAC16_BLACK|(MAC16_RED<<4));
    return false;
  }
  else if(final_flags & TTTP_FLAG_UNICODE) {
    server_socket.Close();
    CLOSE_AUTOPASSFILE();
    display.Statusf("");
    Widgets::ModalInfo(display,
                       "The server requires Unicode mode, but TTTPClient can"
                       " only operate in 8-bit mode. You will need to use a"
                       " different client with this server.",
                       MAC16_BLACK|(MAC16_RED<<4));
    return false;
  }
  else if(final_flags & ~(TTTP_FLAG_ENCRYPTION|TTTP_FLAG_PRECISE_MOUSE)) {
    // these are the only flags we actually support
    server_socket.Close();
    CLOSE_AUTOPASSFILE();
    display.Statusf("");
    Widgets::ModalInfo(display,
                       "The server requires an extension to the TTTP"
                       " protocol which this client does not support."
                       "\n\nTry upgrading to a newer client.",
                       MAC16_BLACK|(MAC16_RED<<4));
    return false;
  }
  if(!no_crypt && !(tttp_client_get_flags(tttp) & TTTP_FLAG_ENCRYPTION)) {
    if(!Widgets::ModalConfirm(display,
                              "The server does not support encryption. This"
                              " connection will NOT be secure.\n\nWould you"
                              " like to continue, even without encryption?")) {
      server_socket.Close();
      CLOSE_AUTOPASSFILE();
      display.Statusf("");
      return false;
    }
  }
  if(!no_auth) {
    tttp_client_begin_handshake(tttp, username.c_str(), public_key);
    do {
      res = tttp_client_pump_auth(tttp);
      switch(res) {
      case TTTP_HANDSHAKE_ADVANCE: break;
      case TTTP_HANDSHAKE_REJECTED:
        server_socket.Close();
        if(username == "")
          Widgets::ModalInfo(display,
                             "This server does not allow guest connections. You probably need to provide a username and password.");
        else
          Widgets::ModalInfo(display,
                             "This server does not support username-based authentication. You might try connecting with an empty username.");
        display.Statusf("");
        return false;
      case TTTP_HANDSHAKE_CONTINUE:
        (void)Net::Select(nullptr,nullptr,nullptr,
                          &socks,nullptr,nullptr,nullptr);
        break;
      default:
        server_socket.Close();
        CLOSE_AUTOPASSFILE();
        display.Statusf("");
        return false;
      }
    } while(res != TTTP_HANDSHAKE_ADVANCE);
    if(autopassfile) {
#ifdef __WIN32__
      password_pointer = reinterpret_cast<const uint8_t*>
        (MapViewOfFile(passhandle, FILE_MAP_READ, 0, 0, passfile_len));
      if(password_pointer == nullptr) {
        Widgets::ModalInfo(display, "Unable to map the password file.", MAC16_BLACK|(MAC16_RED<<4));
        std::cerr << "Unable to map the password file: Error code " << GetLastError() << std::endl;
        }
      password_len = passfile_len;
#else
      password_pointer = reinterpret_cast<const uint8_t*>
        (mmap(nullptr, passfile_len, PROT_READ, MAP_SHARED, passfd, 0));
      if(password_pointer == MAP_FAILED || password_pointer == nullptr) {
        password_pointer = nullptr;
        Widgets::ModalInfo(display, std::string("Unable to map the password file.\n\n")+autopassfile+": "+strerror(errno), MAC16_BLACK|(MAC16_RED<<4));
      }
      password_len = passfile_len;
#endif
      if(password_pointer == nullptr) {
        server_socket.Close();
        CLOSE_AUTOPASSFILE();
        display.Statusf("");
        return false;
      }
    }
    tttp_client_provide_password(tttp, password_pointer, password_len);
    if(autopassfile) {
      UNMAP_AUTOPASSFILE();
    }
    CLOSE_AUTOPASSFILE();
  }
  else tttp_client_begin_handshake(tttp, nullptr, public_key);
  do {
    res = tttp_client_pump_verify(tttp);
    switch(res) {
    case TTTP_HANDSHAKE_ADVANCE: break;
    case TTTP_HANDSHAKE_REJECTED:
      display.Statusf("");
      server_socket.Close();
      if(no_auth)
        Widgets::ModalInfo(display,
                           "This server does not allow connections without"
                           " authentication.");
      else if(!server_sent_key)
        Widgets::ModalInfo(display,
                           "Authentication failed. You probably entered"
                           " an incorrect username or password.\n\n"
                           "However, it's also possible that the public key"
                           " on file for this server is wrong, which could"
                           " mean that the key has been changed or that"
                           " someone is trying (and failing) to impersonate"
                           " this server.\n\n"
                           "If, after several tries, you're sure your username"
                           " and password are correct, consult the server"
                           " operator to see if a new key is needed.\n\n"
                           "The option to change the public key on file for"
                           " a server becomes visible on the main connection"
                           " menu when the " PMOD " key is held.");
      else
        Widgets::ModalInfo(display,
                           "Authentication failed. You entered an incorrect"
                           " username or password.");
      return false;
    case TTTP_HANDSHAKE_CONTINUE:
      (void)Net::Select(nullptr,nullptr,nullptr,
                        &socks,nullptr,nullptr,nullptr);
      break;
    default:
      server_socket.Close();
      display.Statusf("");
      return false;
    }
  } while(res != TTTP_HANDSHAKE_ADVANCE);
  // Connection succeeded!
  display.Statusf("");
  return true;
}
