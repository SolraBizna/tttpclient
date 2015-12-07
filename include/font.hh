#ifndef FONTHH
#define FONTHH

#include "tttpclient.hh"

class Font {
  uint8_t* buffer;
  uint32_t width, height;
public:
  Font(const char* fontpath);
  ~Font();
  uint32_t GetWidth() const { return width; }
  uint32_t GetHeight() const { return height; }
  uint32_t GetGlyphWidth() const { return width / 16; }
  uint32_t GetGlyphHeight() const { return height / 16; }
  const uint8_t*const* GetRows() const { return (const uint8_t*const*)buffer; }
};

#endif
