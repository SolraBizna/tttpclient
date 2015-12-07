#include "tttpclient.hh"
#include "tttp_client.h"
#include "font.hh"
#include "display.hh"
#include "sdlsoft_display.hh"
#include "modal_error.hh"
#include "startup.hh"
#include "mac16.hh"
#include "widgets.hh"
#include "pkdb.hh"

#include <iostream>
#include <lsx.h>

static const char* font_path = nullptr;
static const char* window_title = nullptr;
int queue_depth = -1;
char* autohost = nullptr, *autouser = nullptr, *autopassword = nullptr,
  *autopassfile = nullptr;
bool no_auth = false, no_crypt = false;
Net::SockStream server_socket;
tttp_client* tttp = nullptr;

class LibTTTPInputDelegate : public InputDelegate {
public:
  void Key(int pressed, tttp_scancode scancode) override {
    tttp_client_send_key(tttp, pressed ? TTTP_PRESS : TTTP_RELEASE, scancode);
  }
  void Text(uint8_t* text, size_t textlen) override {
    tttp_client_send_text(tttp, text, textlen);
  }
  void MouseMove(int16_t x, int16_t y) override {
    tttp_client_send_mouse_movement(tttp, x, y);
  }
  void MouseButton(int pressed, uint16_t button) override {
    tttp_client_send_mouse_button(tttp, pressed ? TTTP_PRESS : TTTP_RELEASE,
                                  button);
  }
  void Scroll(int8_t x, int8_t y) override {
    tttp_client_send_scroll(tttp, x, y);
  }
};

static void pltt_callback(void* d, const uint8_t* colors) {
  Display* display = reinterpret_cast<Display*>(d);
  display->SetPalette(colors);
}

static void fram_callback(void* d, uint32_t width, uint32_t height,
                          uint32_t dirty_left, uint32_t dirty_top,
                          uint32_t dirty_width, uint32_t dirty_height,
                          void* framedata) {
  Display* display = reinterpret_cast<Display*>(d);
  display->Update(width, height, dirty_left, dirty_top, dirty_width,
                  dirty_height, reinterpret_cast<uint8_t*>(framedata));
}

static void kick_callback(void* d, const uint8_t* data, size_t len) {
  Display* display = reinterpret_cast<Display*>(d);
  std::string text(reinterpret_cast<const char*>(data), len);
  display->SetPalette(mac16);
  Widgets::ModalInfo(*display,
                     std::string("We were kicked by the server.\n\n")
                     + (len ? text : std::string("No reason was given.")));
  throw quit_exception();
}

static void text_callback(void*, const uint8_t* data, size_t len) {
  std::cout << "TEXT: " << std::string(reinterpret_cast<const char*>(data),
                                       len);
}

extern void die(const char* format, ...) {
  char error[1920]; // enough to fill up an 80x24 terminal
  va_list arg;
  va_start(arg, format);
  vsnprintf(error, sizeof(error), format, arg);
  va_end(arg);
  throw std::string(error);
}

extern "C" tttp_thread_local_block* tttp_get_thread_local_block() {
  static thread_local tttp_thread_local_block b;
  return &b;
}

static int parse_command_line(int argc, char* argv[]) {
  ++argv;
  --argc;
  int ret = 0;
  while(argc > 0) {
    if(**argv == '-') {
      char* arg = *argv;
      ++argv;
      --argc;
      while(*++arg) {
        switch(*arg) {
        case 'q': {
          if(queue_depth >= 0) {
            std::cerr << "-q given more than once" << std::endl;
            ++argv; --argc;
            ret = 1;
            break;
          }
          if(argc <= 0) {
            std::cerr << "No argument given for -q" << std::endl;
            ret = 1;
            break;
          }
          char* endptr;
          errno = 0;
          long l = strtol(*argv, &endptr, 0);
          if(errno != 0 || *endptr || endptr == *argv || l < 0 || l > 255) {
            ++argv; --argc;
            std::cerr << "Argument for -q must be in range 0 -- 255" << std::endl;
            ret = 1;
            break;
          }
          ++argv; --argc;
          queue_depth = l;
        } break;
        case 'h':
          if(argc <= 0) {
            std::cerr << "No argument given for -h" << std::endl;
            ret = 1;
          }
          if(autohost) {
            std::cerr << "-h given more than once" << std::endl;
            ret = 1;
          } else autohost = *argv;
          ++argv; --argc;
          break;
        case 'u':
          if(argc <= 0) {
            std::cerr << "No argument given for -u" << std::endl;
            ret = 1;
          }
          if(autouser) {
            std::cerr << "-u given more than once" << std::endl;
            ret = 1;
          } else autouser = *argv;
          ++argv; --argc;
          break;
        case 'U':
          if(no_auth) {
            std::cerr << "-U given more than once" << std::endl;
            ret = 1;
          }
          if(autopassword || autopassfile) {
            std::cerr << "Only one of -U, -p, -P is allowed" << std::endl;
            ret = 1;
          }
          no_auth = true;
          break;
        case 'E':
          if(no_crypt) {
            std::cerr << "-E given more than once" << std::endl;
            ret = 1;
          }
          no_crypt = true;
          break;
        case 'p':
          if(argc <= 0) {
            std::cerr << "No argument given for -p" << std::endl;
            ret = 1;
          }
          if(autopassfile || no_auth) {
            std::cerr << "Only one of -U, -p, -P is allowed" << std::endl;
            ret = 1;
          }
          if(autopassword) {
            std::cerr << "-p given more than once" << std::endl;
            ret = 1;
          } else autopassword = *argv;
          ++argv; --argc;
          break;
        case 'P':
          if(argc <= 0) {
            std::cerr << "No argument given for -P" << std::endl;
            ret = 1;
          }
          if(autopassword || no_auth) {
            std::cerr << "Only one of -U, -p, -P is allowed" << std::endl;
            ret = 1;
          }
          if(autopassfile) {
            std::cerr << "-P given more than once" << std::endl;
            ret = 1;
          } else autopassfile = *argv;
          ++argv; --argc;
          break;
        case 't':
          if(argc <= 0) {
            std::cerr << "No argument given for -p" << std::endl;
            ret = 1;
          }
          else if(window_title != nullptr) {
            std::cerr << "-t given more than once" << std::endl;
            ret = 1;
          }
          else {
            window_title = *argv++;
            --argc;
          }
          break;
        case 'v':
          std::cout << "TTTPClient " TTTP_CLIENT_VERSION << std::endl;
          return 1;
        }
      }
    }
    else if(font_path == nullptr) {
      font_path = *argv++;
      --argc;
    }
    else {
      std::cerr << "More than one font specified" << std::endl;
      ++argv;
      --argc;
    }
  }
  if(font_path == nullptr) {
    std::cerr << "A font must be specified" << std::endl;
    ret = 1;
  }
  if(ret) {
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  tttpclient <font path> [options...]" << std::endl;
    std::cerr << "The font must be a PNG containing all 256 glyphs of codepage 437, packed" << std::endl;
    std::cerr << "tightly in 8 columns and 8 rows. The glyph size is autodetected." << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -t <title>: Specify a custom window title." << std::endl;
    std::cerr << "  -q <depth>: Queue depth to request. Range is 0-255, default is 0 (server's" << std::endl;
    std::cerr << "discretion)" << std::endl;
    //std::cerr << "  -f: Use fullscreen mode at startup (can always be toggled with alt-enter)" << std::endl;
    std::cerr << "  -h <host>: Automatically connect to a particular host." << std::endl;
    std::cerr << "  -u <user>: Automatically authenticate as a particular user. Provide an empty" << std::endl;
    std::cerr << "username to authenticate as a guest." << std::endl;
    std::cerr << "  -U: Do not attempt authentication. Warning: This also disables encryption!" << std::endl;
    std::cerr << "  -E: Do not attempt encryption. Warning: ONLY use this for connections to" << std::endl;
    std::cerr << "localhost, or connections over a secure tunnel/VPN!" << std::endl;
    std::cerr << "  -p <password>: Automatically authenticate with a given password string." << std::endl;
    std::cerr << "Warning: Other users of the same computer may be able to see your command" << std::endl;
    std::cerr << "lines! Only use this option on single-user computers!" << std::endl;
    std::cerr << "  -P <passpath>: Automatically authenticate, using the contents of the given" << std::endl;
    std::cerr << "file as a password. Warning: This file should only be readable by you, and" << std::endl;
    std::cerr << "should not have a filename others can use to find an identical file, for" << std::endl;
    std::cerr << "reasons given above." << std::endl;
  }
  return ret;
}

int teg_main(int argc, char* argv[]) {
  if(parse_command_line(argc, argv)) return 1;
  if(no_auth) no_crypt = true;
  tttp_init();
  Display* display = nullptr;
  try {
    {
      Font font(font_path);
      display = new SDLSoft_Display(font, window_title ? window_title
                                    : "TTTPClient " TTTP_CLIENT_VERSION);
    }
    display->SetPalette(mac16);
    std::string err;
    if(!no_auth && !PKDB::Init(*display, err)) {
      Widgets::ModalInfo(*display, std::string("Could not initialize the public key database: \"") + err + "\"\n\nAuthentication and encryption are not available.", MAC16_BLACK|(MAC16_RED<<4));
      if(autouser || autopassword || autopassfile) throw quit_exception();
      no_crypt = no_auth = true;
    }
    if(DoConnectionDialog(*display)) {
      LibTTTPInputDelegate del;
      tttp_client_set_core_callbacks(tttp, pltt_callback, fram_callback,
                                     kick_callback);
      tttp_client_set_text_callback(tttp, text_callback);
      display->SetInputDelegate(&del);
      while(tttp_client_pump(tttp))
        display->Pump();
      display->SetPalette(mac16);
      Widgets::ModalInfo(*display,
                         "The connection to the server was closed.");
    }
  }
  catch(std::string s) {
    std::cerr << "There was an unhandled error." << std::endl << std::endl;
    std::cerr << s << std::endl;
    try {
      if(display)
        do_modal_error(*display,
                       std::string("There was an unhandled error.\n\n")+s);
    }
    catch(std::string s2) {
      std::cerr << std::endl << "And an unhandled error while handling the"
        " error." << std::endl << std::endl << s2 << std::endl;
    }
  }
  catch(quit_exception e) {
    (void)e;
    std::cout << "Window was closed." << std::endl;
  }
  if(display != nullptr) delete display;
  return 0;
}
