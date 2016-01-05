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

#ifndef DISPLAYHH
#define DISPLAYHH

#include "tttpclient.hh"
#include "tttp_scancodes.h"

class InputDelegate {
public:
  virtual void Key(int pressed, tttp_scancode scancode) = 0;
  virtual void Text(uint8_t* text, size_t textlen) = 0;
  virtual void MouseMove(int16_t x, int16_t y) = 0;
  virtual void MouseButton(int pressed, uint16_t button) = 0;
  virtual void Scroll(int8_t x, int8_t y) = 0;
};

class DiscardingInputDelegate : public InputDelegate {
public:
  void Key(int pressed, tttp_scancode scancode) override;
  void Text(uint8_t* text, size_t textlen) override;
  void MouseMove(int16_t x, int16_t y) override;
  void MouseButton(int pressed, uint16_t button) override;
  void Scroll(int8_t x, int8_t y) override;
};

class Display {
  InputDelegate* delegate;
  std::string status;
protected:
  uint32_t glyph_width, glyph_height;
  inline const std::string& GetStatusLine() { return status; }
  // if this is called, then the next time Update or Pump is called, the status
  // line must also be updated on screen
  virtual void StatusChanged() = 0;
public:
  static const int MAX_STATUS_LINE_LENGTH = 78;
  Display(uint32_t glyph_width, uint32_t glyph_height);
  virtual ~Display();
  void SetInputDelegate(InputDelegate*);
  void Statusf(const char* format, ...);
  virtual void SetKeyRepeat(uint32_t delay, uint32_t interval) = 0;
  virtual void SetPalette(const uint8_t palette[48]) = 0;
  virtual void Update(uint16_t width, uint16_t height,
                      uint16_t dirty_left, uint16_t dirty_right,
                      uint16_t dirty_width, uint16_t dirty_height,
                      const uint8_t* buffer) = 0;
  // may be called automatically by Update
  // if timeout_ms is <= 0, wait forever
  virtual void Pump(bool wait = false, int timeout_ms = 0) = 0;
  virtual void SetClipboardText(const char*) = 0;
  virtual char* GetClipboardText() = 0; // acts like SDL_GetClipboardText()
  virtual void FreeClipboardText(char*) = 0; // frees pointer returned from ^
  // as above, if there is another clipboard-like thing; defaults to a stub
  // that always returns NULL and does nothing to free
  virtual char* GetOtherClipboardText();
  virtual void FreeOtherClipboardText(char*);
  inline uint32_t GetCharWidth() const { return glyph_width; }
  inline uint32_t GetCharHeight() const { return glyph_height; }
  inline InputDelegate& GetInputDelegate() {
    if(!delegate)
      throw std::string("GetInputDelegate called with no delegate set");
    return *delegate;
  }
};

#endif
