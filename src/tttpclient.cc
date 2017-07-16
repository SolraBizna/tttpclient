/*
  Copyright (C) 2015-2016 Solra Bizna

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

#include "tttpclient.hh"
#include "tttp_scancodes.h"
#include "tttp_client.h"
#include "font.hh"
#include "display.hh"
#include "sdlsoft_display.hh"
#include "modal_error.hh"
#include "startup.hh"
#include "mac16.hh"
#include "widgets.hh"
#include "pkdb.hh"
#include "io.hh"
#include "charconv.hh"

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

static enum class DisplayMode {
  DEFAULT, ACCELERATED
} display_mode = DisplayMode::DEFAULT;
static float max_fps = -1;
static bool have_max_fps = false;

static Display* display = nullptr;
static bool pasting_enabled = false;

class LibTTTPInputDelegate : public InputDelegate {
  bool left_shift_held, right_shift_held;
  bool left_control_held, right_control_held;
  bool left_gui_held, right_gui_held;
  bool left_alt_held, right_alt_held;
  bool ShiftHeld() const { return left_shift_held || right_shift_held; }
  bool ControlHeld() const { return left_control_held || right_control_held; }
  bool GuiHeld() const { return left_gui_held || right_gui_held; }
  bool AltHeld() const { return left_alt_held || right_alt_held; }
  bool CheckMods(bool shift, bool control, bool gui, bool alt) {
    return ShiftHeld() == shift && ControlHeld() == control
      && GuiHeld() == gui && AltHeld() == alt;
  }
public:
  LibTTTPInputDelegate()
    : left_shift_held(false), right_shift_held(false),
      left_control_held(false), right_control_held(false),
      left_gui_held(false), right_gui_held(false),
      left_alt_held(false), right_alt_held(false) {}
  void Key(int pressed, tttp_scancode scancode) override {
    if(scancode == KEY_LEFT_SHIFT) left_shift_held = pressed;
    else if(scancode == KEY_RIGHT_SHIFT) right_shift_held = pressed;
    else if(scancode == KEY_LEFT_CONTROL) left_control_held = pressed;
    else if(scancode == KEY_RIGHT_CONTROL) right_control_held = pressed;
    else if(scancode == KEY_LEFT_GUI) left_gui_held = pressed;
    else if(scancode == KEY_RIGHT_GUI) right_gui_held = pressed;
    else if(scancode == KEY_LEFT_ALT) left_alt_held = pressed;
    else if(scancode == KEY_RIGHT_ALT) right_alt_held = pressed;
    // scroll lock is supposed to be a toggle, so a physical "press" could
    // generate either a press or a release; allow both to quit
    else if(pressed || scancode == KEY_SCROLL_LOCK) {
      /* QUIT
         All platforms: Control+Backslash, Control+Pause, Control+ScrollLock
         Mac: Command+Q, Command+W
         Non-Mac: Alt+F4
      */
      if((scancode == KEY_BACKSLASH && CheckMods(false,true,false,false))
         || (scancode == KEY_PAUSE && CheckMods(false,true,false,false))
         || (scancode == KEY_SCROLL_LOCK && CheckMods(false,true,false,false))
#if MACOSX
         || ((scancode == KEY_W || scancode == KEY_Q)
             && CheckMods(false,false,true,false))
#else
         || (scancode == KEY_F4 && CheckMods(false,false,false,true))
#endif
         ) {
        throw quit_exception();
      }
      /*
        PASTE
        All platforms: Shift+Insert
        Mac: Command+V
        Non-Mac: Control+V
       */
      else if(pasting_enabled
         && ((scancode == KEY_INSERT && CheckMods(true,false,false,false))
#if MACOSX
             || (scancode == KEY_V && CheckMods(false,false,true,false))
#else
             || (scancode == KEY_V && CheckMods(false,true,false,false))
#endif
             )) {
        char* cbt = display->GetClipboardText();
        tttp_client_begin_paste(tttp);
        if(cbt) {
          auto cbtlen = strlen(cbt);
          // overwrite the buffer as we go, it's okay
          uint8_t* cop = convert_utf8_to_cp437(reinterpret_cast<uint8_t*>(cbt),
                                               reinterpret_cast<uint8_t*>(cbt),
                                               cbtlen,
                                               [this](uint8_t* sofar,
                                                      size_t sofarlen,
                                                      tttp_scancode code){
                                                 Text(sofar, sofarlen);
                                                 if(code == KEY_ENTER
                                                    || code == KEY_TAB) {
                                                   Key(1, code); Key(0, code);
                                                 }
                                               });
          auto outlen = cop - reinterpret_cast<uint8_t*>(cbt);
          if(outlen > 0) Text(reinterpret_cast<uint8_t*>(cbt), outlen);
          display->FreeClipboardText(cbt);
        }
        tttp_client_end_paste(tttp);
        return;
      }
    }
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
    if(pasting_enabled) {
      /* there are six instances of slightly different code to do this in this
         program, I should really have made this generic */
      char* cbt = display->GetOtherClipboardText();
      if(cbt && *cbt != 0) {
        tttp_client_begin_paste(tttp);
        auto cbtlen = strlen(cbt);
        //overwrite the buffer as we go, it's okay, CP437 is shorter than UTF-8
        uint8_t* cop = convert_utf8_to_cp437(reinterpret_cast<uint8_t*>(cbt),
                                             reinterpret_cast<uint8_t*>(cbt),
                                             cbtlen,
                                             [this](uint8_t* sofar,
                                                    size_t sofarlen,
                                                    tttp_scancode code){
                                               Text(sofar, sofarlen);
                                               if(code == KEY_ENTER
                                                  || code == KEY_TAB) {
                                                 Key(1, code); Key(0, code);
                                               }
                                             });
        auto outlen = cop - reinterpret_cast<uint8_t*>(cbt);
        if(outlen > 0) Text(reinterpret_cast<uint8_t*>(cbt), outlen);
        tttp_client_end_paste(tttp);
      }
      if(cbt) display->FreeOtherClipboardText(cbt);
    }
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

static void pmode_callback(void*, int enabled) {
  pasting_enabled = enabled;
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
        default:
          std::cerr << "Unknown option: " << *arg << std::endl;
          // fall through
        case '?':
          ret = 1;
          break;
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
        case 'a':
          if(display_mode != DisplayMode::DEFAULT) {
            // TODO: when we implement other display modes, be disgruntled
          }
          if(display_mode == DisplayMode::ACCELERATED) {
            std::cerr << "-a given more than once" << std::endl;
            ret = 1;
          }
          else display_mode = DisplayMode::ACCELERATED;
          break;
        case 'v':
          std::cout << "TTTPClient " TTTP_CLIENT_VERSION << std::endl;
          return 1;
        case 'V':
          if(have_max_fps) {
            std::cerr << "multiple framerate strategies given" << std::endl;
            ret = 1;
          }
          else {
            max_fps = -1;
            have_max_fps = true;
          }
          break;
        case 'r': {
          if(have_max_fps) {
            std::cerr << "multiple framerate strategies given" << std::endl;
            ret = 1;
            ++argv; --argc;
            break;
          }
          if(argc <= 0) {
            std::cerr << "No argument given for -r" << std::endl;
            ret = 1;
            break;
          }
          char* endptr;
          errno = 0;
          float f = strtof(*argv, &endptr);
          if(errno != 0 || *endptr || endptr == *argv
             || ((f < 0.1f || f > 1000) && f != 0.f)){
            ++argv; --argc;
            std::cerr << "Argument for -r must be in range 0.1 -- 1000, or be 0" << std::endl;
            ret = 1;
            break;
          }
          ++argv; --argc;
          max_fps = f;
          have_max_fps = true;
        } break;
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
    std::cerr << "tightly in 8 columns and 8 rows. The glyph size is autodetected, and may not" << std::endl;
    std::cerr << "be greater than 255x255." << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -t <title>: Specify a custom window title." << std::endl;
    std::cerr << "  -a: Try basic hardware acceleration. May cause problems with some drivers." << std::endl;
    std::cerr << "  -r <framerate>: Limit framerate to a specified value between 0.1 and 1000." << std::endl;
    std::cerr << "  -r 0: Unlimited framerate." << std::endl;
    std::cerr << "  -V: Try to synchronize framerate to monitor. (default)" << std::endl;
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
#if __WIN32__
  IO::DoRedirectOutput();
#endif
  if(parse_command_line(argc, argv)) return 1;
  if(no_auth) no_crypt = true;
  tttp_init();
  try {
    {
      Font font(font_path);
      display = new SDLSoft_Display(font, window_title ? window_title
                                    : "TTTPClient " TTTP_CLIENT_VERSION,
                                    display_mode == DisplayMode::ACCELERATED,
                                    max_fps);
    }
    display->SetPalette(mac16);
    std::string err;
    if(!no_auth && !PKDB::Init(*display, err)) {
      Widgets::ModalInfo(*display, std::string("Could not initialize the public key database: \"") + err + "\"\n\nAuthentication and encryption are not available.", MAC16_BLACK|(MAC16_RED<<4));
      if(autouser || autopassword || autopassfile) throw quit_exception();
      no_crypt = no_auth = true;
    }
    if(DoConnectionDialog(*display)) {
      PKDB::Fini();
      LibTTTPInputDelegate del;
      tttp_client_set_core_callbacks(tttp, pltt_callback, fram_callback,
                                     kick_callback);
      tttp_client_set_text_callback(tttp, text_callback);
      tttp_client_set_paste_mode_callback(tttp, pmode_callback);
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
  catch(quit_exception) { /* quit silently */ }
  catch(std::exception& e) {
    std::cerr << "There was an unhandled exception." << std::endl << std::endl;
    std::cerr << e.what() << std::endl;
    try {
      if(display)
        do_modal_error(*display,
                       std::string("There was an unhandled exception.\n\n")+e.what());
    }
    catch(std::string s2) {
      std::cerr << std::endl << "And an unhandled error while handling the"
        " exception." << std::endl << std::endl << s2 << std::endl;
    }
  }
  if(display != nullptr) delete display;
  return 0;
}
