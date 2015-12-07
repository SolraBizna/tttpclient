#ifndef SDLSOFTDISPLAYHH
#define SDLSOFTDISPLAYHH

#include "display.hh"
#include "font.hh"

class SDLSoft_Display : public Display {
  bool status_dirty, exposed, has_alpha, has_color;
  uint8_t* glyphdata;
  uint32_t glyph_width, glyph_height;
  uint32_t glyphpitch; // bytes between GLYPHS, not ROWS of glyphs
  uint16_t cur_width, cur_height, dirty_left, dirty_top, dirty_right,dirty_bot;
  uint16_t prev_status_len;
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* frametexture;
  SDL_Texture* statustexture;
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
  SDLSoft_Display(Font& font, const char* title);
  ~SDLSoft_Display() override;
  void SetKeyRepeat(uint32_t delay, uint32_t interval) override;
  void SetPalette(const uint8_t palette[48]) override;
  void Update(uint16_t width, uint16_t height,
              uint16_t dirty_left, uint16_t dirty_top,
              uint16_t dirty_width, uint16_t dirty_height,
              const uint8_t* buffer) override;
  void Pump(bool wait = false) override;
  void SetClipboardText(const char*) override;
  char* GetClipboardText() override;
  void FreeClipboardText(char*) override;
  char* GetOtherClipboardText() override;
  void FreeOtherClipboardText(char*) override;
};

#endif
