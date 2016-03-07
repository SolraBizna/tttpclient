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

#ifndef SDLSOFTDISPLAYHH
#define SDLSOFTDISPLAYHH

#include "display.hh"
#include "font.hh"

#include <chrono>

class SDLSoft_Display : public Display {
  typedef std::chrono::steady_clock clock;
  bool throttle_framerate;
  clock::time_point next_frame;
  clock::duration frame_interval;
  bool status_dirty, exposed, has_alpha, has_color;
  uint8_t* glyphdata;
  uint32_t glyphpitch; // bytes between GLYPHS, not ROWS of glyphs
  uint16_t cur_width, cur_height, dirty_left, dirty_top, dirty_right,dirty_bot;
  uint16_t prev_status_len;
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* frametexture;
  SDL_Texture* statustexture;
  SDL_Texture* overlaytexture;
  int overlay_x, overlay_y, overlay_w, overlay_h;
  int overlay_source_w, overlay_source_h;
  uint8_t palette[48];
  void DrawGlyph(uint8_t color, uint8_t glyph,
                 uint8_t* outbase, uint32_t pitch,
                 const uint8_t* palette);
  void DrawGlyphA(uint8_t color, uint8_t glyph,
                  uint8_t* outbase, uint32_t pitch,
                  const uint8_t* palette);
  void DrawGlyphC(uint8_t color, uint8_t glyph,
                  uint8_t* outbase, uint32_t pitch,
                  const uint8_t* palette);
  void DrawGlyphAC(uint8_t color, uint8_t glyph,
                   uint8_t* outbase, uint32_t pitch,
                   const uint8_t* palette);
  void UpdateTextureWithPixels(SDL_Texture* target,
                               uint32_t start_x, uint32_t start_y,
                               uint32_t end_x, uint32_t end_y,
                               uint32_t datapitch,
                               const uint8_t* colorstart,
                               const uint8_t* charstart,
                               const uint8_t* palette);
protected:
  void StatusChanged() override;
public:
  // max_fps < 0 := try to do vsync
  // max_fps == 0 := unlimited framerate
  SDLSoft_Display(Font& font, const char* title, bool accel, float max_fps);
  ~SDLSoft_Display() override;
  void SetKeyRepeat(uint32_t delay, uint32_t interval) override;
  void SetPalette(const uint8_t palette[48]) override;
  void Update(uint16_t width, uint16_t height,
              uint16_t dirty_left, uint16_t dirty_top,
              uint16_t dirty_width, uint16_t dirty_height,
              const uint8_t* buffer) override;
  void Pump(bool wait = false, int timeout_ms = 0) override;
  void SetClipboardText(const char*) override;
  char* GetClipboardText() override;
  void FreeClipboardText(char*) override;
  char* GetOtherClipboardText() override;
  void FreeOtherClipboardText(char*) override;
  void SetOverlayTexture(SDL_Texture* tex, int w, int h);
  void SetOverlayRegion(int x, int y, int w, int h);
  inline SDL_Renderer* GetRenderer() const { return renderer; }
};

#endif
