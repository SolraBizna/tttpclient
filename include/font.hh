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
