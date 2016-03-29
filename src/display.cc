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

#include "display.hh"

#include <iostream>

Display::Display(uint32_t glyph_width, uint32_t glyph_height)
  : delegate(NULL), glyph_width(glyph_width), glyph_height(glyph_height)  {
  if(glyph_width == 0 || glyph_width > 255
     || glyph_height == 0 || glyph_height > 255)
    throw std::string("Absurd font size");
}

Display::~Display() {}

void Display::SetInputDelegate(InputDelegate* delegate) {
  this->delegate = delegate;
}

void Display::Statusf(const char* format, ...) {
  char buf[MAX_STATUS_LINE_LENGTH+1];
  va_list arg;
  va_start(arg, format);
  vsnprintf(buf, sizeof(buf), format, arg);
  va_end(arg);
  std::string nu(buf);
  if(nu != status) {
    status = std::move(nu);
    StatusChanged();
  }
  if(delegate == nullptr) {
    // nobody is listening, force an update
    DiscardingInputDelegate del;
    delegate = &del;
    Pump();
    delegate = nullptr;
  }
}

void DiscardingInputDelegate::Key(int, tttp_scancode) {}
void DiscardingInputDelegate::Text(uint8_t*, size_t) {}
void DiscardingInputDelegate::MouseMove(int16_t, int16_t) {}
void DiscardingInputDelegate::MouseButton(int, uint16_t) {}
void DiscardingInputDelegate::Scroll(int8_t, int8_t) {}

char* Display::GetOtherClipboardText() { return nullptr; }
void Display::FreeOtherClipboardText(char*) {}
