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
#include "font.hh"
#include "display.hh"
#include "sdlsoft_display.hh"
#include "modal_error.hh"
#include "startup.hh"
#include "widgets.hh"
#include "mac16.hh"
#include "io.hh"

#include <iostream>
#include <chrono>

#include "paint_framebuffer.hh"

static const char* font_path = "VGA8x16.png";
static const char* overlay_path = nullptr;
static std::string save_path;
static enum class DisplayMode {
  DEFAULT, ACCELERATED
} display_mode = DisplayMode::DEFAULT;
static int chars_wide = 80, chars_high = 24, undo_depth = 100;

static const int MIN_CHARS_HIGH = 30;

static const int FOREGROUND_COLOR_Y = 1;
static const int BACKGROUND_COLOR_Y = 3;
static const int GLYPHS_Y = 6;
static const int KEYS_Y = 24;

static const uint8_t OVERLAY_MIN_ALPHA = 0;
static const uint8_t OVERLAY_STARTING_ALPHA = 63;
static const uint8_t OVERLAY_MAX_ALPHA = 254; // 255 hits a bug in SDL 2.0.2
static const int OVERLAY_PULSE_PERIOD_NUM = 5;
static const int OVERLAY_PULSE_PERIOD_DEN = 2;
static const int OVERLAY_MICROSECONDS_PER_PULSE_CHANGE
= (long long)OVERLAY_PULSE_PERIOD_NUM*1000000/(OVERLAY_PULSE_PERIOD_DEN*(OVERLAY_MAX_ALPHA-OVERLAY_MIN_ALPHA+1)*2);

static const uint8_t DIVIDER_COLOR = 0x08;
static const uint8_t TEXT_COLOR = 0x70;
static const uint8_t DISABLED_COLOR = 0x80;
static const uint8_t UNSELECTED_GLYPH_COLOR = 0x70;
static const uint8_t SELECTED_GLYPH_COLOR = 0x0F;

static const uint8_t DIVIDER_GLYPH = 0xF6;
static const uint8_t UNSELECTED_COLOR_GLYPH = 0x00;
static const uint8_t SELECTED_COLOR_GLYPH = 0x07;

static const int PAINT_GLYPH = -1;

static SDLSoft_Display* display = nullptr;
static SDL_Texture* overlay = nullptr;
static Framebuffer* canvas;
static Framebuffer* undo_stack = nullptr;
static int overlay_width, overlay_height;

static uint8_t fgcolor = 15;
static uint8_t bgcolor = 0;
static int selected_glyph = PAINT_GLYPH;
static bool overlay_on = true, overlay_pulsing = false,
  overlay_pulse_polarity;
static int overlay_alpha = OVERLAY_STARTING_ALPHA;
static std::chrono::steady_clock::time_point overlay_pulse_tick;

static int mouse_x_glyph = -1;
static int mouse_y_glyph = -1;
static int mouse_y_halfglyph = -1;
static bool painting = false, unpainting = false;

static int newest_undo = 0, oldest_undo = 0, cur_undo = 0, saved_undo = 0;
static bool undo_was_okay = false, redo_was_okay = false,
  canvas_was_unsaved = false, canvas_is_unsaved = false;

extern SDL_Texture* png_to_sdl_texture(SDL_Renderer* renderer,
                                       const char* path,
                                       int& out_width, int& out_height);

static InputDelegate& save_fb() {
  auto& ret = display->GetInputDelegate();
  display->SetPalette(mac16);
  display->SetOverlayTexture(nullptr, 0, 0);
  return ret;
}

static void restore_fb(InputDelegate& del) {
  if(overlay_on)
    display->SetOverlayTexture(overlay, overlay_width, overlay_height);
  if(overlay_pulsing)
    overlay_pulse_tick = std::chrono::steady_clock::now();
  display->SetInputDelegate(&del);
  display->SetPalette(tttp_default_palette);
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
        case 'w': {
          if(argc <= 0) {
            std::cerr << "No argument given for -w" << std::endl;
            ret = 1;
            break;
          }
          char* endptr;
          errno = 0;
          long l = strtol(*argv, &endptr, 0);
          if(errno != 0 || *endptr || endptr == *argv || l <= 0 || l > 254) {
            ++argv; --argc;
            std::cerr << "Argument for -w must be in range 1 -- 254" << std::endl;
            ret = 1;
            break;
          }
          ++argv; --argc;
          chars_wide = l;
        } break;
        case 'h': {
          if(argc <= 0) {
            std::cerr << "No argument given for -h" << std::endl;
            ret = 1;
            break;
          }
          char* endptr;
          errno = 0;
          long l = strtol(*argv, &endptr, 0);
          if(errno != 0 || *endptr || endptr == *argv || l <= 0 || l > 254) {
            ++argv; --argc;
            std::cerr << "Argument for -h must be in range 1 -- 254" << std::endl;
            ret = 1;
            break;
          }
          ++argv; --argc;
          chars_high = l;
        } break;
        case 'f': {
          if(argc <= 0) {
            std::cerr << "No argument given for -f" << std::endl;
            ret = 1;
            break;
          }
          font_path = *argv++;
          --argc;
        } break;
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
        case 'u': {
          if(argc <= 0) {
            std::cerr << "No argument given for -u" << std::endl;
            ret = 1;
            break;
          }
          char* endptr;
          errno = 0;
          long l = strtol(*argv, &endptr, 0);
          if(errno != 0 || *endptr || endptr == *argv || l < 0 || l > 65535) {
            ++argv; --argc;
            std::cerr << "Argument for -u must be in range 0 -- 65535" << std::endl;
            ret = 1;
            break;
          }
          ++argv; --argc;
          // undo_depth includes the current canvas
          if(l != 0) undo_depth = l + 1;
          else undo_depth = l;
        } break;
        case 'v':
          std::cout << "Paint, from TTTPClient " TTTP_CLIENT_VERSION << std::endl;
          return 1;
        }
      }
    }
    else if(strlen(*argv) > 4
            && !strcasecmp(*argv + strlen(*argv) - 4, ".png")) {
      if(overlay_path == nullptr) {
        overlay_path = *argv++;
        --argc;
      }
      else {
        std::cerr << "More than one overlay specified" << std::endl;
        ++argv;
        --argc;
      }
    }
    else if(strlen(*argv) > 4
            && !strcmp(*argv + strlen(*argv) - 4, ".frm")) {
      if(save_path.size() == 0) {
        save_path = *argv++;
        --argc;
      }
      else {
        std::cerr << "More than one save file specified" << std::endl;
        ++argv;
        --argc;
      }
    }
    else {
      std::cerr << "Invalid path (does not end in .png or .frm)" <<std::endl;
      ++argv;
      --argc;
      ret = 1;
    }
  }
  if(font_path == nullptr) {
    std::cerr << "A font must be specified" << std::endl;
    ret = 1;
  }
  if(ret) {
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  paint [overlay.png] [result.frm] [options...]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -a: Try basic hardware acceleration. May cause problems with some drivers." << std::endl;
    std::cerr << "  -f <font>: Use a different font. Font must be tttpclient compatible." << std::endl;
    std::cerr << "  -w <width>: Use a screen width other than 80 chars. Ignored if a .frm is" << std::endl;
    std::cerr << "loaded." << std::endl;
    std::cerr << "  -h <height>: Use a screen height other than 24 chars. Ignored if a .frm is" << std::endl;
    std::cerr << "loaded." << std::endl;
    std::cerr << "  -u <count>: Number of undo levels to keep (default 100)" << std::endl;
  }
  return ret;
}

static void edit_canvas() {
  canvas_is_unsaved = true;
  if(undo_depth > 0) {
    int next_undo = cur_undo + 1;
    if(next_undo >= undo_depth) next_undo -= undo_depth;
    undo_stack[next_undo] = undo_stack[cur_undo];
    cur_undo = next_undo;
    newest_undo = cur_undo;
    if(oldest_undo == newest_undo) {
      if(oldest_undo == saved_undo) saved_undo = -1;
      oldest_undo = oldest_undo + 1;
      if(oldest_undo >= undo_depth) oldest_undo -= undo_depth;
    }
    canvas = undo_stack + next_undo;
  }
}

static void paint_divider(Framebuffer& fb) {
  uint8_t* cp = fb.GetColorPointer(chars_wide, 0);
  uint8_t* gp = fb.GetGlyphPointer(chars_wide, 0);
  for(int y = 0; y < fb.GetHeight(); ++y) {
    *cp = DIVIDER_COLOR; *gp = DIVIDER_GLYPH;
    cp += fb.GetPitch(); gp += fb.GetPitch();
  }
  if(chars_high < MIN_CHARS_HIGH) {
    cp = fb.GetColorPointer(0, chars_high);
    gp = fb.GetGlyphPointer(0, chars_high);
    for(int x = 0; x < chars_wide; ++x) {
      *cp++ = DIVIDER_COLOR; *gp++ = DIVIDER_GLYPH;
    }
  }
}

static void paint_glyphs(Framebuffer& fb) {
  uint8_t c = 0;
  for(int y = 0; y < 16; ++y) {
    uint8_t* cp = fb.GetColorPointer(chars_wide + 1, GLYPHS_Y+y);
    uint8_t* gp = fb.GetGlyphPointer(chars_wide + 1, GLYPHS_Y+y);
    for(int x = 0; x < 16; ++x) {
      *cp++ = UNSELECTED_GLYPH_COLOR; *gp++ = c++;
    }
  }
}

static void paint_colors(Framebuffer& fb, int x, int y) {
  uint8_t* cp = fb.GetColorPointer(x, y);
  uint8_t* gp = fb.GetGlyphPointer(x, y);
  *cp++ = 0x70; *gp++ = UNSELECTED_COLOR_GLYPH;
  for(uint8_t c = 1; c < 16; ++c) {
    *cp++ = c; *gp++ = UNSELECTED_COLOR_GLYPH;
  }
}

static void poke_color_selection(Framebuffer& fb, int y, uint8_t color,
                                 uint8_t glyph) {
  *fb.GetGlyphPointer(chars_wide+1+color, y) = glyph;
  fb.DirtyPoint(chars_wide+1+color, y);
}

static void poke_ui_region_color(Framebuffer& fb, int y, uint8_t color) {
  auto p = fb.GetColorPointer(chars_wide+1, y);
  memset(p, color, 16);
  fb.DirtyRect(chars_wide+1, y, 16, 1);
}

static void poke_glyph_selection(Framebuffer& fb, int selection,uint8_t color){
  if(selection >= 0) {
    int x = chars_wide+1+(selection&15);
    int y = GLYPHS_Y+(selection>>4);
    *fb.GetColorPointer(x, y) = color;
    fb.DirtyPoint(x, y);
  }
  else {
    poke_ui_region_color(fb, GLYPHS_Y+16, color);
  }
}

static void init_screen(Framebuffer& fb) {
  paint_divider(fb);
  paint_glyphs(fb);
  paint_colors(fb, chars_wide+1, FOREGROUND_COLOR_Y);
  paint_colors(fb, chars_wide+1, BACKGROUND_COLOR_Y);
  fb.PrintStr(chars_wide + 1, FOREGROUND_COLOR_Y-1, TEXT_COLOR, "Foreground:");
  fb.PrintStr(chars_wide + 1, BACKGROUND_COLOR_Y-1, TEXT_COLOR, "Background:");
  fb.PrintStr(chars_wide + 1, GLYPHS_Y-1, TEXT_COLOR, "Glyph:");
  fb.PrintStr(chars_wide + 2, GLYPHS_Y+16, UNSELECTED_GLYPH_COLOR,
              "pai[n]t");
  fb.PrintStr(chars_wide + 1, KEYS_Y+0, DISABLED_COLOR, "S: save");
  fb.PrintStr(chars_wide + 1, KEYS_Y+1, overlay_path != nullptr ? TEXT_COLOR
              : DISABLED_COLOR, "P: pulse o-lay");
  fb.PrintStr(chars_wide + 1, KEYS_Y+2, overlay_path != nullptr ? TEXT_COLOR
              : DISABLED_COLOR, "O: toggle o-lay");
  fb.PrintStr(chars_wide + 1, KEYS_Y+3, overlay_path != nullptr ? TEXT_COLOR
              : DISABLED_COLOR, "0-9: o-lay alpha");
  fb.PrintStr(chars_wide + 1, KEYS_Y+4, DISABLED_COLOR, "Z: undo");
  fb.PrintStr(chars_wide + 1, KEYS_Y+5, DISABLED_COLOR, "Y: redo");
  // if I don't display it, nobody will ever know that I planned it
  //fb.PrintStr(chars_wide + 1, KEYS_Y+5, TEXT_COLOR, "F: fill");
  poke_color_selection(fb, FOREGROUND_COLOR_Y, fgcolor, SELECTED_COLOR_GLYPH);
  poke_color_selection(fb, BACKGROUND_COLOR_Y, bgcolor, SELECTED_COLOR_GLYPH);
  poke_glyph_selection(fb, selected_glyph, SELECTED_GLYPH_COLOR);
}

static void paint(int glyph, int x, int y, bool erasing) {
  if(x < 0 || x >= chars_wide || y < 0 || y >= chars_high*2) return;
  if(glyph == PAINT_GLYPH) {
    auto color = erasing ? bgcolor : fgcolor;
    uint8_t* cp = canvas->GetColorPointer(x, y/2);
    uint8_t* gp = canvas->GetGlyphPointer(x, y/2);
    if(*gp == 0) {
      if((*cp&15) != color) {
        *cp = (y&1)?((color<<4)|(*cp&15)):(color|(*cp<<4));
        *gp = 0xDC;
      }
    }
    else if(*gp == 0xDC) {
      if(y&1) {
        if((*cp&15) == color) {
          *cp = color;
          *gp = 0;
        }
        else
          *cp = (*cp&0x0F)|(color<<4);
      }
      else {
        if((*cp>>4) == color) {
          *cp = color;
          *gp = 0;
        }
        else
          *cp = (*cp&0xF0)|color;
      }
    }
    else {
      *cp = (y&1)?((color<<4)|(*cp&15)):(color|(*cp<<4));
      *gp = 0xDC;
    }
    canvas->DirtyPoint(x,y/2);
  }
  else {
    y /= 2;
    *canvas->GetColorPointer(x, y) = (fgcolor<<4)|bgcolor;
    *canvas->GetGlyphPointer(x, y) = erasing ? 0 : glyph;
    canvas->DirtyPoint(x,y);
  }
}

static void paint_path(int glyph, int x, int y, int ex, int ey,
                       bool erasing) {
  paint(glyph, x, y, erasing);
  if(x == ex) {
    if(y == ey) return;
    if(ey < y) std::swap(y, ey);
    do { paint(glyph, x, ++y, erasing); } while(y != ey);
  }
  else if(y == ey) {
    if(ex < x) std::swap(x, ex);
    do { paint(glyph, ++x, y, erasing); } while(x != ex);
  }
  else {
    int dx = (ex-x);
    if(dx < 0) dx = -dx;
    int dy = (ey-y);
    if(dy < 0) dy = -dy;
    if(dx > dy) {
      if(ex < x) std::swap(x, ex), std::swap(y, ey);
      int n = dy;
      int d = dx;
      int r = n/2;
      int yd = ey<y?-1:1;
      while(x++ != ex) {
        r += n;
        if(r >= d) r -= d, y += yd;
        paint(glyph, x, y, erasing);
      }
    }
    else {
      if(ey < y) std::swap(x, ex), std::swap(y, ey);
      int n = dx;
      int d = dy;
      int r = n/2;
      int xd = ex<x?-1:1;
      while(y++ != ey) {
        r += n;
        if(r >= d) r -= d, x += xd;
        paint(glyph, x, y, erasing);
      }
    }
  }
}

static void save(Framebuffer& fb) {
  if(save_path.size() == 0) {
    display->Pump(false);
    auto& _ = save_fb();
    Widgets::Container container(*display, 80, 7);
    bool save = false, done = false;
    std::shared_ptr<Widgets::LabeledField> filename_widget
      (new Widgets::LabeledField(container, 4, 2, 72, 19,
                                 "Filename to save to", "painting.frm"));
    std::shared_ptr<Widgets::Button> save_button
      (new Widgets::Button(container, 56, 4, 20,
                           "Save", [&save,&done] { done = save = true; }));
    std::shared_ptr<Widgets::Button> cancel_button
      (new Widgets::Button(container, 34, 4, 20,
                           "Cancel", [&done] { done = true; }));
    container.AddWidget(filename_widget);
    container.AddWidget(cancel_button);
    container.AddWidget(save_button);
    filename_widget->SetTabNeighbor(cancel_button)
      ->SetTabNeighbor(save_button)->SetTabNeighbor(filename_widget);
    save_button->SetUpNeighbor(filename_widget);
    cancel_button->SetDownNeighbor(filename_widget);
    save_button->SetRightNeighbor(cancel_button)->SetRightNeighbor(save_button)
      ->SetLeftNeighbor(cancel_button)->SetLeftNeighbor(save_button);
    container.FocusFirstEnabledWidget();
    do {
      container.RunModal([&done] { return done; });
      auto s = filename_widget->GetContent();
      if(s.size() < 4 || s.substr(s.size()-4, s.size()) != ".frm") {
        done = false;
        Widgets::ModalInfo(*display, "Save file name must end in \".frm\".");
      }
      else {
        FILE* f = fopen(s.c_str(), "rb");
        if(f != nullptr) {
          fclose(f);
          if(!Widgets::ModalConfirm(*display, "A file with that name already exists. Overwrite it?"))
            done = false;
        }
      }
    } while(!done);
    restore_fb(_);
    fb.DirtyWhole();
    if(!save) return;
    save_path = filename_widget->GetContent();
  }
  FILE* f = fopen(save_path.c_str(), "wb");
  if(f == nullptr) {
    auto& _ = save_fb();
    Widgets::ModalInfo(*display, std::string("Couldn't save: ") + strerror(errno));
    restore_fb(_);
    fb.DirtyWhole();
    return;
  }
  fputc(chars_wide, f);
  fputc(chars_high, f);
  if(fwrite(canvas->GetBuffer(), 1, canvas->GetWidth()*canvas->GetHeight()*2,f)
     != size_t(canvas->GetWidth()*canvas->GetHeight()*2)) {
    auto& _ = save_fb();
    Widgets::ModalInfo(*display, std::string("Couldn't write: ") + strerror(errno));
    restore_fb(_);
    fb.DirtyWhole();
  }
  else {
    canvas_is_unsaved = false;
    saved_undo = cur_undo;
  }
  fclose(f);
}

int teg_main(int argc, char* argv[]) {
#if __WIN32__
  IO::DoRedirectOutput();
#endif
  if(parse_command_line(argc, argv)) return 1;
  try {
    {
      bool load_failed = false, ignore_load_failure = false;
      std::string load_failed_why;
      Framebuffer loaded;
      if(save_path.size() > 0) {
        FILE* f = fopen(save_path.c_str(), "rb");
        if(f == nullptr) {
          load_failed = true;
          load_failed_why = strerror(errno);
          ignore_load_failure = errno == ENOENT;
        }
        else {
          int wide = fgetc(f);
          int high = fgetc(f);
          if(wide == EOF || wide == 0 || high == EOF || high == 0) {
            load_failed = true;
            load_failed_why = "Not a valid FRM";
          }
          else if(wide == 255 || high == 255) {
            load_failed = true;
            load_failed_why = "FRM is too large";
          }
          else {
            loaded.Resize(wide, high);
            if(fread(loaded.GetBuffer(), 1, wide*high*2, f)
               != size_t(wide*high*2)) {
              load_failed = true;
              load_failed_why = "Unexpected EOF";
            }
            else {
              chars_wide = wide;
              chars_high = high;
            }
          }
          fclose(f);
        }
      }
      if(undo_depth > 0) {
        undo_stack = new Framebuffer[undo_depth]();
        for(int n = 0; n < undo_depth; ++n) undo_stack[n].Resize(chars_wide,
                                                                 chars_high,
                                                                 n != 0);
        canvas = undo_stack + cur_undo;
      }
      else canvas = new Framebuffer(chars_wide, chars_high);
      if(save_path.size() > 0 && !load_failed)
        *canvas = loaded;
      if(load_failed && !ignore_load_failure) {
        display->SetPalette(mac16);
        Widgets::ModalInfo(*display,
                           std::string("Unable to load the FRM file: ")
                           + load_failed_why);
      }
      Font font(font_path);
      display = new SDLSoft_Display(font, "Paint from TTTPClient "
                                    TTTP_CLIENT_VERSION,
                                    display_mode == DisplayMode::ACCELERATED,
                                    0);
      SDL_RendererInfo info;
      SDL_version vers;
      SDL_GetRendererInfo(display->GetRenderer(), &info);
      SDL_GetVersion(&vers);
      std::cout << "Paint from TTTPClient version " TTTP_CLIENT_VERSION << std::endl;
      std::cout << "SDL version: " << (int)vers.major << "." << (int)vers.minor
                << "." << (int)vers.patch << std::endl;
      std::cout << "Using driver: " << info.name << std::endl;
      std::cout << "Canvas is " << chars_wide << " x " << chars_high << std::endl;
    }
    if(overlay_path != nullptr) {
      SDL_SetHint("SDL_HINT_RENDER_SCALE_QUALITY", "1");
      overlay = png_to_sdl_texture(display->GetRenderer(), overlay_path,
                                   overlay_width, overlay_height);
      SDL_SetHint("SDL_HINT_RENDER_SCALE_QUALITY", "0");
      int target_width = chars_wide * display->GetCharWidth();
      int target_height = chars_high * display->GetCharHeight();
      if(overlay_width != target_width || overlay_height != target_height) {
        display->SetPalette(mac16);
        std::string prompt = TEG::format("The canvas is %i x %i pixels, but"
                                         " the overlay image is %i x %i"
                                         " pixels. For best results, the"
                                         " overlay and the canvas should be"
                                         " the same size. \n\nContinue?",
                                         target_width, target_height,
                                         overlay_width, overlay_height);
        if(!Widgets::ModalConfirm(*display, prompt))
          throw quit_exception();
        double xr = (double)target_width / overlay_width;
        double yr = (double)target_height / overlay_height;
        if(xr > yr) {
          int scaled_width = overlay_width * target_height / overlay_height;
          display->SetOverlayRegion((target_width-scaled_width)/2, 0,
                                    scaled_width, target_height);
        }
        else {
          int scaled_height = overlay_height * target_width / overlay_width;
          display->SetOverlayRegion(0, (target_height-scaled_height)/2,
                                    target_width, scaled_height);
        }
      }
      else
        display->SetOverlayRegion(0, 0, target_width, target_height);
      SDL_SetTextureBlendMode(overlay, SDL_BLENDMODE_BLEND);
      if(SDL_SetTextureAlphaMod(overlay, OVERLAY_STARTING_ALPHA))
        throw std::string("Can't set alpha modulation with this renderer");
      display->SetOverlayTexture(overlay, overlay_width, overlay_height);
    }
    display->SetPalette(tttp_default_palette);
    Framebuffer fb(chars_wide + 17, chars_high < MIN_CHARS_HIGH ? MIN_CHARS_HIGH : chars_high);
    init_screen(fb);
    class LocalInputDelegate : public InputDelegate {
      Framebuffer& fb;
    public:
      LocalInputDelegate(Framebuffer& fb) : fb(fb) {}
      void Key(int pressed, tttp_scancode scancode) override {
        if(!pressed) return;
        switch(scancode) {
        case KEY_N:
          if(selected_glyph != PAINT_GLYPH) {
            poke_glyph_selection(fb, selected_glyph, UNSELECTED_GLYPH_COLOR);
            selected_glyph = PAINT_GLYPH;
            poke_glyph_selection(fb, selected_glyph, SELECTED_GLYPH_COLOR);
          }
          break;
        case KEY_0:
          // 255 breaks software renderer in 2.0.2
          SDL_SetTextureAlphaMod(overlay, 254);
          if(overlay_on) fb.DirtyWhole();
          break;
        case KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5: case KEY_6:
        case KEY_7: case KEY_8: case KEY_9:
          SDL_SetTextureAlphaMod(overlay, (overlay_alpha = (scancode-KEY_0)*255/10));
          if(overlay_on) fb.DirtyWhole();
          break;
        case KEY_O:
          if(overlay == nullptr) break;
          overlay_on = !overlay_on;
          overlay_pulsing = false;
          if(overlay_on) {
            // assume success
            display->SetOverlayTexture(overlay, overlay_width, overlay_height);
          }
          else
            display->SetOverlayTexture(nullptr, 0, 0);
          fb.DirtyRect(0, 0, chars_wide, chars_high);
          break;
        case KEY_P:
          if(overlay == nullptr) break;
          if(overlay_pulsing) {
            overlay_pulsing = false;
            fb.DirtyRect(0, 0, chars_wide, chars_high);
          }
          else {
            if(!overlay_on)
              display->SetOverlayTexture(overlay,
                                         overlay_width, overlay_height);
            overlay_on = true;
            overlay_pulsing = true;
            overlay_pulse_polarity = true;
            overlay_pulse_tick = std::chrono::steady_clock::now();
          }
          break;
        case KEY_Z:
          if(cur_undo != oldest_undo) {
            cur_undo = cur_undo - 1;
            if(cur_undo < 0) cur_undo += undo_depth;
            canvas = undo_stack + cur_undo;
            canvas->DirtyWhole();
            canvas_is_unsaved = cur_undo != saved_undo;
          }
          break;
        case KEY_Y:
          if(cur_undo != newest_undo) {
            cur_undo = cur_undo + 1;
            if(cur_undo >= undo_depth) cur_undo -= undo_depth;
            canvas = undo_stack + cur_undo;
            canvas->DirtyWhole();
            canvas_is_unsaved = cur_undo != saved_undo;
          }
          break;
        case KEY_S:
          save(fb);
          break;
        default: break;
        }
      }
      void Text(uint8_t*, size_t) override {}
      void MouseMove(int16_t x, int16_t y) override {
        if(painting || unpainting)
          paint_path(selected_glyph,
                     x / display->GetCharWidth(),
                     y * 2 / display->GetCharHeight(),
                     mouse_x_glyph, mouse_y_halfglyph,
                     unpainting);
        mouse_x_glyph = x / display->GetCharWidth();
        mouse_y_glyph = y / display->GetCharHeight();
        mouse_y_halfglyph = y * 2 / display->GetCharHeight();
      }
      void MouseButton(int pressed, uint16_t button) override {
        if(painting) {
          if(!pressed && button == TTTP_LEFT_MOUSE_BUTTON)
            painting = false;
        }
        else if(unpainting) {
          if(!pressed && button == TTTP_RIGHT_MOUSE_BUTTON)
            unpainting = false;
        }
        else if(pressed && button == TTTP_RIGHT_MOUSE_BUTTON) {
          if(mouse_x_glyph >= 0 && mouse_x_glyph < chars_wide
             && mouse_y_glyph >= 0 && mouse_y_glyph < chars_high) {
            edit_canvas();
            unpainting = true;
            paint(selected_glyph, mouse_x_glyph, mouse_y_halfglyph, true);
          }
        }
        else if(pressed && button == TTTP_LEFT_MOUSE_BUTTON) {
          if(mouse_x_glyph >= 0 && mouse_x_glyph < chars_wide
             && mouse_y_glyph >= 0 && mouse_y_glyph < chars_high) {
            edit_canvas();
            painting = true;
            paint(selected_glyph, mouse_x_glyph, mouse_y_halfglyph, false);
          }
          else if(mouse_x_glyph >= chars_wide+1
                  && mouse_x_glyph <= fb.GetWidth()
                  && mouse_y_glyph >= 0 && mouse_y_glyph < chars_high) {
            int x = mouse_x_glyph - chars_wide - 1;
            int y = mouse_y_glyph;
            if(y == FOREGROUND_COLOR_Y) {
              uint8_t nu = x;
              if(nu != fgcolor) {
                poke_color_selection(fb, FOREGROUND_COLOR_Y,
                                     fgcolor, UNSELECTED_COLOR_GLYPH);
                fgcolor = nu;
                poke_color_selection(fb, FOREGROUND_COLOR_Y,
                                     fgcolor, SELECTED_COLOR_GLYPH);
              }
            }
            else if(y == BACKGROUND_COLOR_Y) {
              uint8_t nu = x;
              if(nu != bgcolor) {
                poke_color_selection(fb, BACKGROUND_COLOR_Y,
                                     bgcolor, UNSELECTED_COLOR_GLYPH);
                bgcolor = nu;
                poke_color_selection(fb, BACKGROUND_COLOR_Y,
                                     bgcolor, SELECTED_COLOR_GLYPH);
              }
            }
            else if(y == GLYPHS_Y + 16) {
              if(selected_glyph != PAINT_GLYPH) {
                poke_glyph_selection(fb,selected_glyph,UNSELECTED_GLYPH_COLOR);
                selected_glyph = PAINT_GLYPH;
                poke_glyph_selection(fb, selected_glyph, SELECTED_GLYPH_COLOR);
              }
            }
            else if(y < GLYPHS_Y + 16 && y >= GLYPHS_Y) {
              int nu = ((y - GLYPHS_Y) << 4) | x;
              if(selected_glyph != nu) {
                poke_glyph_selection(fb,selected_glyph,UNSELECTED_GLYPH_COLOR);
                selected_glyph = nu;
                poke_glyph_selection(fb, selected_glyph, SELECTED_GLYPH_COLOR);
              }
            }
          }
        }
        else if(pressed && button == TTTP_MIDDLE_MOUSE_BUTTON) {
          /* TODO: try to paste from selection buffer */
        }
      }
      void Scroll(int8_t, int8_t) override {}
    } del(fb);
    display->SetInputDelegate(&del);
    while(1) {
      canvas->Update(fb);
      if(overlay_on && overlay_pulsing) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>
          (now-overlay_pulse_tick).count()/OVERLAY_MICROSECONDS_PER_PULSE_CHANGE;
        overlay_pulse_tick += std::chrono::microseconds(diff*OVERLAY_MICROSECONDS_PER_PULSE_CHANGE);
        if(overlay_pulse_polarity) overlay_alpha += diff;
        else overlay_alpha -= diff;
        while(overlay_pulse_polarity ? overlay_alpha > OVERLAY_MAX_ALPHA
              : overlay_alpha < OVERLAY_MIN_ALPHA) {
          if(overlay_pulse_polarity)
            overlay_alpha += OVERLAY_MAX_ALPHA - overlay_alpha;
          else
            overlay_alpha = -overlay_alpha;
          overlay_pulse_polarity = !overlay_pulse_polarity;
        }
        SDL_SetTextureAlphaMod(overlay, overlay_alpha);
        fb.DirtyRect(0, 0, chars_wide, chars_high);
      }
      if(canvas_is_unsaved != canvas_was_unsaved) {
        poke_ui_region_color(fb, KEYS_Y+0,
                             canvas_is_unsaved?TEXT_COLOR:DISABLED_COLOR);
        canvas_was_unsaved = canvas_is_unsaved;
      }
      bool undo_is_okay = cur_undo != oldest_undo;
      bool redo_is_okay = cur_undo != newest_undo;
      if(undo_is_okay != undo_was_okay) {
        poke_ui_region_color(fb, KEYS_Y+4,
                             undo_is_okay?TEXT_COLOR:DISABLED_COLOR);
        undo_was_okay = undo_is_okay;
      }
      if(redo_is_okay != redo_was_okay) {
        poke_ui_region_color(fb, KEYS_Y+5,
                             redo_is_okay?TEXT_COLOR:DISABLED_COLOR);
        redo_was_okay = redo_is_okay;
      }
      fb.Update(*display);
      try {
        display->Pump(true, overlay_pulsing ? 10 : 0);
      }
      catch(quit_exception) {
        if(canvas_is_unsaved) {
          auto& _ = save_fb();
          if(Widgets::ModalConfirm(*display,
                                   "Save changes before quitting?")) {
            restore_fb(_);
            save(fb);
          }
        }
        throw;
      }
    }
  }
  catch(std::string s) {
    if(display)
      display->SetOverlayTexture(nullptr, 0, 0);
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
    if(display)
      display->SetOverlayTexture(nullptr, 0, 0);
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
