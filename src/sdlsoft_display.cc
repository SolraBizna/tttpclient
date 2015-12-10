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

#include "sdlsoft_display.hh"
#include "charconv.hh"

#if defined(SDL_VIDEO_DRIVER_X11)
# define Font UnConflictMe
# define Display X11Display
# include "SDL_syswm.h"
# include <X11/Xatom.h>
# undef Font
# undef Display
#include <vector>
#endif

extern "C" const uint8_t blend_table[16*64*64];

static uint8_t fisqrt(uint8_t in) {
  uint8_t x = 8, n = 8;
  do {
    n >>= 1;
    int16_t s = (x*x) - (int16_t)in;
    if(s == 0) return x;
    else if(s > 0) x -= n;
    else if(s < 0) x += n;
  } while(n > 0);
  if(x*x > in) return x-1; else return x;
}

template<bool has_alpha, bool has_colors>
void copy_out_glyph_data(uint32_t glyph_width, uint32_t glyph_height,
                         uint8_t* outp, const uint8_t*const* rows) {
  for(unsigned int glyph = 0; glyph < 256; ++glyph) {
    uint32_t base_x = (glyph % 16) * glyph_width;
    uint32_t base_y = (glyph / 16) * glyph_height;
    for(uint32_t y = 0; y < glyph_height; ++y) {
      const uint8_t* inp = rows[base_y + y] + base_x * 4;
      for(uint32_t x = 0; x < glyph_width; ++x) {
        *outp++ = fisqrt(*inp++);
        if(has_colors) { *outp++ = fisqrt(*inp++); *outp++ = fisqrt(*inp++); }
        else inp += 2;
        if(has_alpha) *outp++ = *inp++ >> 4;
        else ++inp;
      }
    }
  }
}

SDLSoft_Display::SDLSoft_Display(Font& font, const char* title)
  : status_dirty(false), exposed(false),
    cur_width(0), cur_height(0), prev_status_len(0), frametexture(NULL) {
  if(SDL_Init(SDL_INIT_VIDEO)) throw std::string(SDL_GetError());
  glyph_width = font.GetGlyphWidth();
  glyph_height = font.GetGlyphHeight();
  glyphpitch = glyph_width * glyph_height;
  if(glyphpitch / glyph_height != glyph_width)
    throw std::string("really improbable integer overflow");
  // has_alpha: A other than 1
  // has_color: in some pixel with A != 0, R != G or R != B
  // has_blending: A other than 1/0 or R/G/B other than 1/0 (not used)
  has_alpha = false; has_color = false; /*has_blending = false;*/
  for(uint32_t y = 0; y < font.GetHeight(); ++y) {
    const uint8_t* p = font.GetRows()[y];
    for(uint32_t x = 0; x < font.GetWidth(); ++x) {
      uint8_t r = *p++>>4; uint8_t g = *p++>>4; uint8_t b = *p++>>4;
      uint8_t a = *p++>>4;
      if(a == 0) has_alpha = true;
      else {
        if(a != 15) { has_alpha = true; /*has_blending = true;*/ }
        if(r != g || g != b) { has_color = true; /*has_blending = true;*/ }
        else if(r != 15) { /*has_blending = true;*/ }
      }
    }
    if(has_alpha && has_color /*&& has_blending*/) break;
  }
  uint32_t mult = 1;
  if(has_alpha) ++mult;
  if(has_color) mult += 2;
  if(glyphpitch * mult * 256 / mult / 256 != glyphpitch)
    throw std::string("really improbable integer overflow");
  glyphpitch *= mult;
  glyphdata = (uint8_t*)safe_malloc(glyphpitch * 256);
  if(has_alpha) {
    if(has_color)
      copy_out_glyph_data<true, true>(glyph_width, glyph_height,
                                      glyphdata, font.GetRows());
    else
      copy_out_glyph_data<true, false>(glyph_width, glyph_height,
                                       glyphdata, font.GetRows());
  }
  else {
    if(has_color)
      copy_out_glyph_data<false, true>(glyph_width, glyph_height,
                                       glyphdata, font.GetRows());
    else
      copy_out_glyph_data<false, false>(glyph_width, glyph_height,
                                        glyphdata, font.GetRows());
  }
  // the connection dialogue is 80x9, save us having to resize the window
  window = SDL_CreateWindow(title,
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            80 * glyph_width, 9 * glyph_height,
                            0);
  if(window == NULL) {
    SDL_Quit();
    safe_free(glyphdata);
    throw std::string(SDL_GetError());
  }
  renderer = SDL_CreateRenderer(window, -1, 0); // TODO: SDL_RENDERER_SOFTWARE
  if(renderer == NULL) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    safe_free(glyphdata);
    throw std::string(SDL_GetError());
  }
  statustexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                    SDL_TEXTUREACCESS_STREAMING, glyph_width
                                    * Display::MAX_STATUS_LINE_LENGTH,
                                    glyph_height);
  if(statustexture == NULL) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    safe_free(glyphdata);
    throw std::string(SDL_GetError());
  }
}

SDLSoft_Display::~SDLSoft_Display() {
  if(frametexture) SDL_DestroyTexture(frametexture);
  if(renderer) SDL_DestroyRenderer(renderer);
  if(window) SDL_DestroyWindow(window);
  if(glyphdata) safe_free(glyphdata);
  SDL_Quit();
}

void SDLSoft_Display::StatusChanged() {
  status_dirty = true;
}

void SDLSoft_Display::SetKeyRepeat(uint32_t delay, uint32_t interval) {
  // SDL 2 doesn't have this
  // TODO: Implement this
  (void)delay; (void)interval;
}

void SDLSoft_Display::SetPalette(const uint8_t palette[48]) {
  memcpy(this->palette, palette, 48);
  dirty_left = 0; dirty_top = 0;
  dirty_right = cur_width-1; dirty_bot = cur_height-1;
}

void SDLSoft_Display::DrawGlyph(uint8_t color, uint8_t glyph,
                                uint8_t* outbase, uint32_t pitch,
                                const uint8_t* palette) {
  uint8_t* fontp = glyphdata + glyph * glyphpitch;
  uint8_t bg = color&15;
  uint8_t bg_r = palette[bg*3];
  uint8_t bg_g = palette[bg*3+1];
  uint8_t bg_b = palette[bg*3+2];
  uint8_t fg = color>>4;
  uint8_t fg_r = palette[fg*3];
  uint8_t fg_g = palette[fg*3+1];
  uint8_t fg_b = palette[fg*3+2];
  uint32_t bg_pix = (bg_r << 16) | (bg_g << 8) | bg_b;
  uint32_t fg_pix = (fg_r << 16) | (fg_g << 8) | fg_b;
  for(uint32_t y = 0; y < glyph_height; ++y) {
    uint32_t* outp = (uint32_t*)outbase;
    outbase += pitch;
    for(uint32_t x = 0; x < glyph_width; ++x) {
      uint8_t l = *fontp++;
      if(l == 0) *outp++ = bg_pix;
      else if(l == 15) *outp++ = fg_pix;
      else {
        uint8_t out_r = blend_table[((bg_r>>2)<<10)
                                    |(fg_r&240)
                                    |l];
        uint8_t out_g = blend_table[((bg_g>>2)<<10)
                                    |(fg_g&240)
                                    |l];
        uint8_t out_b = blend_table[((bg_b>>2)<<10)
                                    |(fg_b&240)
                                    |l];
        *outp++ = (out_r<<16)|(out_g<<8)|out_b;
      }
    }
  }
}

void SDLSoft_Display::DrawGlyphA(uint8_t color, uint8_t glyph,
                                 uint8_t* outbase, uint32_t pitch,
                                 const uint8_t* palette) {
  uint8_t* fontp = glyphdata + glyph * glyphpitch;
  uint8_t bg = color&15;
  uint8_t bg_r = palette[bg*3];
  uint8_t bg_g = palette[bg*3+1];
  uint8_t bg_b = palette[bg*3+2];
  uint8_t fg = color>>4;
  uint8_t fg_r = palette[fg*3];
  uint8_t fg_g = palette[fg*3+1];
  uint8_t fg_b = palette[fg*3+2];
  uint32_t bg_pix = (bg_r << 16) | (bg_g << 8) | bg_b;
  uint32_t fg_pix = (fg_r << 16) | (fg_g << 8) | fg_b;
  for(uint32_t y = 0; y < glyph_height; ++y) {
    uint32_t* outp = (uint32_t*)outbase;
    outbase += pitch;
    for(uint32_t x = 0; x < glyph_width; ++x) {
      uint8_t l = *fontp++;
      uint8_t a = *fontp++;
      if(a == 0) *outp++ = bg_pix;
      else if(a == 15) {
        if(l == 15) *outp++ = fg_pix;
        else if(l == 0) *outp++ = 0;
        else {
          uint8_t out_r = blend_table[(fg_r>>2<<4)
                                      |l];
          uint8_t out_g = blend_table[(fg_g>>2<<4)
                                      |l];
          uint8_t out_b = blend_table[(fg_b>>2<<4)
                                      |l];
          *outp++ = (out_r<<16)|(out_g<<8)|out_b;
        }
      }
      else {
        uint8_t out_r = blend_table[((bg_r>>2)<<10)
                                    |(blend_table[(fg_r>>2<<4)
                                                  |l]>>2<<4)|a];
        uint8_t out_g = blend_table[((bg_g>>2)<<10)
                                    |(blend_table[(fg_g>>2<<4)
                                                  |l]>>2<<4)|a];
        uint8_t out_b = blend_table[((bg_b>>2)<<10)
                                    |(blend_table[(fg_b>>2<<4)
                                                  |l]>>2<<4)|a];
        *outp++ = (out_r<<16)|(out_g<<8)|out_b;
      }
    }
  }
}

void SDLSoft_Display::DrawGlyphC(uint8_t color, uint8_t glyph,
                                 uint8_t* outbase, uint32_t pitch,
                                 const uint8_t* palette) {
  uint8_t* fontp = glyphdata + glyph * glyphpitch;
  uint8_t bg = color&15;
  uint8_t bg_r = palette[bg*3];
  uint8_t bg_g = palette[bg*3+1];
  uint8_t bg_b = palette[bg*3+2];
  uint8_t fg = color>>4;
  uint8_t fg_r = palette[fg*3];
  uint8_t fg_g = palette[fg*3+1];
  uint8_t fg_b = palette[fg*3+2];
  uint32_t bg_pix = (bg_r << 16) | (bg_g << 8) | bg_b;
  uint32_t fg_pix = (fg_r << 16) | (fg_g << 8) | fg_b;
  for(uint32_t y = 0; y < glyph_height; ++y) {
    uint32_t* outp = (uint32_t*)outbase;
    outbase += pitch;
    for(uint32_t x = 0; x < glyph_width; ++x) {
      uint8_t r = *fontp++;
      uint8_t g = *fontp++;
      uint8_t b = *fontp++;
      uint8_t tc = r+g+b;
      if(tc == 0) *outp++ = bg_pix;
      else if(tc == 45) *outp++ = fg_pix;
      else {
        uint8_t out_r = blend_table[((bg_r>>2)<<10)
                                    |(fg_r&240)
                                    |r];
        uint8_t out_g = blend_table[((bg_g>>2)<<10)
                                    |(fg_g&240)
                                    |g];
        uint8_t out_b = blend_table[((bg_b>>2)<<10)
                                    |(fg_b&240)
                                    |b];
        *outp++ = (out_r<<16)|(out_g<<8)|out_b;
      }
    }
  }
}

void SDLSoft_Display::DrawGlyphAC(uint8_t color, uint8_t glyph,
                                  uint8_t* outbase, uint32_t pitch,
                                  const uint8_t* palette) {
  uint8_t* fontp = glyphdata + glyph * glyphpitch;
  uint8_t bg = color&15;
  uint8_t bg_r = palette[bg*3];
  uint8_t bg_g = palette[bg*3+1];
  uint8_t bg_b = palette[bg*3+2];
  uint8_t fg = color>>4;
  uint8_t fg_r = palette[fg*3];
  uint8_t fg_g = palette[fg*3+1];
  uint8_t fg_b = palette[fg*3+2];
  uint32_t bg_pix = (bg_r << 16) | (bg_g << 8) | bg_b;
  uint32_t fg_pix = (fg_r << 16) | (fg_g << 8) | fg_b;
  for(uint32_t y = 0; y < glyph_height; ++y) {
    uint32_t* outp = (uint32_t*)outbase;
    outbase += pitch;
    for(uint32_t x = 0; x < glyph_width; ++x) {
      uint8_t r = *fontp++;
      uint8_t g = *fontp++;
      uint8_t b = *fontp++;
      uint8_t a = *fontp++;
      uint8_t tc = r+g+b;
      if(a == 0) *outp++ = bg_pix;
      else if(a == 15) {
        if(tc == 45) *outp++ = fg_pix;
        else if(tc == 0) *outp++ = 0;
        else {
          uint8_t out_r = blend_table[(fg_r>>2<<4)
                                      |r];
          uint8_t out_g = blend_table[(fg_g>>2<<4)
                                      |g];
          uint8_t out_b = blend_table[(fg_b>>2<<4)
                                      |b];
          *outp++ = (out_r<<16)|(out_g<<8)|out_b;
        }
      }
      else {
        uint8_t out_r = blend_table[((bg_r>>2)<<10)
                                    |(blend_table[(fg_r>>2<<4)
                                                  |r]>>2<<4)|a];
        uint8_t out_g = blend_table[((bg_g>>2)<<10)
                                    |(blend_table[(fg_g>>2<<4)
                                                  |g]>>2<<4)|a];
        uint8_t out_b = blend_table[((bg_b>>2)<<10)
                                    |(blend_table[(fg_b>>2<<4)
                                                  |b]>>2<<4)|a];
        *outp++ = (out_r<<16)|(out_g<<8)|out_b;
      }
    }
  }
}

void SDLSoft_Display::UpdateTextureWithPixels(SDL_Texture* target,
                                              uint32_t start_x,
                                              uint32_t start_y,
                                              uint32_t end_x,
                                              uint32_t end_y,
                                              uint32_t datapitch,
                                              const uint8_t* colorstart,
                                              const uint8_t* charstart,
                                              const uint8_t* palette) {
  void* locked_p; int pixelpitch;
  SDL_Rect r = {(int)(start_x * glyph_width),
                (int)(start_y * glyph_height),
                (int)((end_x - start_x + 1) * glyph_width),
                (int)((end_y - start_y + 1) * glyph_height)};
  SDL_LockTexture(target, &r, &locked_p, &pixelpitch);
  uint8_t* fbpos = (uint8_t*)locked_p;
  void (SDLSoft_Display::*glyph_drawing_func)(uint8_t color, uint8_t glyph,
                                              uint8_t* outbase,uint32_t pitch,
                                              const uint8_t* palette);
  if(has_alpha) {
    if(has_color) glyph_drawing_func = &SDLSoft_Display::DrawGlyphAC;
    else glyph_drawing_func = &SDLSoft_Display::DrawGlyphA;
  }
  else {
    if(has_color) glyph_drawing_func = &SDLSoft_Display::DrawGlyphC;
    else glyph_drawing_func = &SDLSoft_Display::DrawGlyph;
  }
  for(uint32_t y = start_y; y <= end_y; ++y) {
    const uint8_t* colorp = colorstart;
    colorstart += datapitch;
    const uint8_t* glyphp = charstart;
    charstart += datapitch;
    uint8_t* outbase = fbpos;
    fbpos += pixelpitch * glyph_height;
    for(uint32_t x = start_x; x <= end_x; ++x) {
      uint8_t color = *colorp++;
      uint8_t glyph = *glyphp++;
      (this->*glyph_drawing_func)(color, glyph, outbase, pixelpitch, palette);
      outbase += glyph_width*4;
    }
  }
  SDL_UnlockTexture(target);
}

void SDLSoft_Display::Update(uint16_t width, uint16_t height,
                             uint16_t dirty_left, uint16_t dirty_top,
                             uint16_t dirty_width, uint16_t dirty_height,
                             const uint8_t* buffer) {
  if(width != cur_width || height != cur_height) {
    cur_width = width; cur_height = height;
    if(frametexture) SDL_DestroyTexture(frametexture);
    int pw, ph;
    SDL_GetWindowSize(window, &pw, &ph);
    if(pw != (int)(width * glyph_width) || ph != (int)(height * glyph_height)){
      SDL_SetWindowSize(window, width * glyph_width, height * glyph_height);
      do {
        this->dirty_left = cur_width; this->dirty_right = 0;
        this->dirty_top = cur_height; this->dirty_bot = 0;
        exposed = false;
        status_dirty = false;
        Pump(true);
      } while(!exposed);
      status_dirty = true;
    }
    SDL_RenderClear(renderer);
    frametexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width * glyph_width,
                                     height * glyph_height);
    if(!frametexture) throw std::string(SDL_GetError());
    exposed = true;
    dirty_left = 0; dirty_top = 0;
    dirty_width = width; dirty_height = height;
    this->dirty_left = width; this->dirty_right = 0;
    this->dirty_top = height; this->dirty_bot = 0;
  }
  if(dirty_width == 0 || dirty_height == 0) return; // nothing to do
  uint16_t dirty_right = dirty_left + dirty_width - 1;
  uint16_t dirty_bot = dirty_top + dirty_height - 1;
  if(dirty_left < this->dirty_left) this->dirty_left = dirty_left;
  if(dirty_right > this->dirty_right) this->dirty_right = dirty_right;
  if(dirty_top < this->dirty_top) this->dirty_top = dirty_top;
  if(dirty_bot > this->dirty_bot) this->dirty_bot = dirty_bot;
  UpdateTextureWithPixels(frametexture,
                          dirty_left, dirty_top,
                          dirty_right, dirty_bot,
                          width,
                          buffer + width * dirty_top + dirty_left,
                          buffer+(width*height)+width*dirty_top+dirty_left,
                          palette);
  Pump();
}

static const uint8_t status_palette[] = {0x00, 0x00, 0x00, 0xff, 0xff, 0xff};
static const uint8_t status_colors[Display::MAX_STATUS_LINE_LENGTH+2] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

void SDLSoft_Display::Pump(bool wait) {
  bool need_present = exposed;
  if(dirty_left <= dirty_right && dirty_top <= dirty_bot) {
    if(status_dirty && dirty_bot >= cur_height-1
       && GetStatusLine().length() < prev_status_len) {
      uint32_t new_status_width = GetStatusLine().length();
      if(new_status_width > 0) new_status_width += 2;
      if(dirty_left > new_status_width) dirty_left = new_status_width;
      if(dirty_right < prev_status_len+1)
        dirty_right = prev_status_len+1;
    }
    SDL_Rect region = {(int)(dirty_left * glyph_width),
                       (int)(dirty_top * glyph_height),
                       (int)((dirty_right - dirty_left + 1) * glyph_width),
                       (int)((dirty_bot - dirty_top + 1) * glyph_height)};
    SDL_RenderCopy(renderer, frametexture, &region, &region);
    need_present = true;
  }
  if(status_dirty || dirty_bot >= cur_height-1) {
    if(status_dirty) {
      if(dirty_bot < cur_height - 1 && GetStatusLine().length() < prev_status_len){
        uint32_t new_status_width = GetStatusLine().length();
        if(new_status_width > 0) new_status_width += 2;
        SDL_Rect region = {(int)(new_status_width * glyph_width),
                           (int)((cur_height-1) * glyph_height),
                           (int)((prev_status_len+1) * glyph_width),
                           (int)(glyph_height)};
        SDL_RenderCopy(renderer, frametexture, &region, &region);
        need_present = true;
      }
      if(GetStatusLine().length() > 0) {
        uint8_t buf[Display::MAX_STATUS_LINE_LENGTH+2];
        buf[0] = 0;
        buf[GetStatusLine().length()+1] = 0;
        memcpy(buf+1, GetStatusLine().data(), GetStatusLine().length());
        UpdateTextureWithPixels(statustexture,
                                0, 0, GetStatusLine().length()+1, 0, 0,
                                status_colors, buf, status_palette);
      }
    }
    if(GetStatusLine().length() > 0) {
      SDL_Rect to_region = {0, (int)((cur_height-1) * glyph_height),
                            (int)((GetStatusLine().length()+2) * glyph_width),
                            (int)(glyph_height)};
      SDL_Rect from_region = {0, 0, (int)((GetStatusLine().length()+2) *
                                          glyph_width),
                              (int)glyph_height};
      SDL_RenderCopy(renderer, statustexture, &from_region, &to_region);
      need_present = true;
    }
    prev_status_len = GetStatusLine().length();
    status_dirty = false;
  }
  if(need_present) SDL_RenderPresent(renderer);
  exposed = false;
  dirty_left = cur_width; dirty_top = cur_height;
  dirty_right = 0; dirty_bot = 0;
  SDL_Event evt;
  while(wait ? SDL_WaitEvent(&evt) : SDL_PollEvent(&evt)) {
    switch(evt.type) {
    case SDL_QUIT: throw quit_exception(); break;
    case SDL_WINDOWEVENT:
      switch(evt.window.event) {
        //case SDL_WINDOWEVENT_SHOWN: visible = true; break;
      //case SDL_WINDOWEVENT_HIDDEN: visible = false; break;
      case SDL_WINDOWEVENT_EXPOSED: exposed = true; wait = false; break;
        //case SDL_WINDOWEVENT_MINIMIZED: minimized = true; break;
        //case SDL_WINDOWEVENT_MAXIMIZED: minimized = false; break;
        //case SDL_WINDOWEVENT_RESTORED: minimized = false; break;
        // SDL_WINDOWEVENT_CLOSED will send SDL_QUIT event
      }
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      {
        auto sym = evt.key.keysym.sym;
        uint16_t scancode;
        if((sym >= 8 && sym <= 10) || sym == 27 || (sym >= 0x20 && sym<=0x7F)){
          scancode = sym;
          if(scancode >= 'a' && scancode <= 'z') scancode -= 0x20;
        }
        else {
          switch(evt.key.keysym.scancode) {
          case SDL_SCANCODE_RETURN: scancode = KEY_ENTER; break;
          case SDL_SCANCODE_ESCAPE: scancode = KEY_ESCAPE; break;
          case SDL_SCANCODE_TAB: scancode = KEY_TAB; break;
          case SDL_SCANCODE_BACKSPACE: scancode = KEY_BACKSPACE; break;
          case SDL_SCANCODE_DELETE: scancode = KEY_DELETE; break;
          default:
            scancode = (evt.key.keysym.scancode & 0xFFFF) + 128;
          }
        }
        GetInputDelegate().Key(evt.type == SDL_KEYDOWN, (tttp_scancode)scancode);
        wait = false;
      }
      break;
    case SDL_MOUSEMOTION:
      GetInputDelegate().MouseMove(evt.motion.x / glyph_width,
                               evt.motion.y / glyph_height);
      wait = false;
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      GetInputDelegate().MouseButton(evt.type == SDL_MOUSEBUTTONDOWN,
                                 evt.button.button - 1);
      wait = false;
      break;
    case SDL_MOUSEWHEEL:
      // breaks down with very rapid scrolling
      GetInputDelegate().Scroll(evt.wheel.x, evt.wheel.y);
      wait = false;
      break;
    case SDL_TEXTINPUT:
      {
        uint8_t buf[sizeof(evt.text.text)+1];
        uint8_t* outp = convert_utf8_to_cp437((const uint8_t*)evt.text.text,
                                              buf,
                                              strlen(evt.text.text),
                                         [this](uint8_t* sofar,
                                                size_t sofarlen,
                                                tttp_scancode scancode) {
                                           GetInputDelegate().Text(sofar,
                                                                   sofarlen);
                                           GetInputDelegate().Key(1, scancode);
                                           GetInputDelegate().Key(0, scancode);
                                              });
        if(outp != buf) GetInputDelegate().Text(buf, outp-buf);
        wait = false;
      }
      break;
    }
  }
}

void SDLSoft_Display::SetClipboardText(const char* text) {
  SDL_SetClipboardText(text);
}

char* SDLSoft_Display::GetClipboardText() {
  return SDL_GetClipboardText();
}

void SDLSoft_Display::FreeClipboardText(char* ptr) {
  SDL_free(ptr);
}

char* SDLSoft_Display::GetOtherClipboardText() {
#if defined(SDL_VIDEO_DRIVER_X11)
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if(SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_X11) {
    /* Try using xclip to read the selection. Don't try very hard. */
    FILE* xclip = popen("xclip -o -selection primary", "r");
    if(xclip) {
      std::vector<char> text;
      char buf[512];
      size_t red;
      while((red = fread(buf, 1, sizeof(buf), xclip)) > 0) {
        text.insert(text.end(), buf, buf + red);
      }
      pclose(xclip);
      if(text.size() > 0) {
        char* ret = reinterpret_cast<char*>(safe_malloc(text.size()+1));
        memcpy(ret, text.data(), text.size());
        ret[text.size()] = 0;
        return ret;
      }
    }
  }
#endif
  return nullptr;
}

void SDLSoft_Display::FreeOtherClipboardText(char* ptr) {
  safe_free(ptr);
}
