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

#include "modal_error.hh"
#include "mac16.hh"
#include "break_lines.hh"

#include <vector>
#include <iostream>

#define LEFT_PADDING 2
#define RIGHT_PADDING 2
#define TOP_PADDING 3
#define BOT_PADDING 3

#define WIDTH 80
#define LINE_WIDTH (WIDTH-LEFT_PADDING-RIGHT_PADDING)

#include <chrono>

static const std::chrono::high_resolution_clock::duration SAFETY_PERIOD
= std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>
  (std::chrono::milliseconds(500));

class ModalErrorDelegate : public InputDelegate{
  std::chrono::high_resolution_clock::time_point safe_time;
  bool exited;
public:
  bool DidExit() const { return exited; }
  ModalErrorDelegate()
    : safe_time(std::chrono::high_resolution_clock::now() + SAFETY_PERIOD),
      exited(false) {}
  void Key(int pressed, tttp_scancode) override {
    if(!pressed) return;
    if(std::chrono::high_resolution_clock::now() >= safe_time) {
      exited = true;
    }
  }
  void Text(uint8_t*, size_t) override {}
  void MouseMove(int16_t, int16_t) override {}
  void MouseButton(int, uint16_t) override {}
  void Scroll(int8_t, int8_t) override {}
};

void do_modal_error(Display& display, const std::string& _message) {
  std::string message = _message + "\n\nPress any key to exit.";
  display.SetInputDelegate(nullptr);
  display.Statusf("");
  display.SetPalette(mac16);
  std::vector<std::string> lines = break_lines(message, LINE_WIDTH);
  const uint32_t HEIGHT = TOP_PADDING + BOT_PADDING + lines.size();
  uint8_t framebuffer[WIDTH*HEIGHT*2];
  memset(framebuffer, MAC16_RED | (MAC16_BLACK << 4), WIDTH*HEIGHT);
  memset(framebuffer + WIDTH*HEIGHT, 0, WIDTH*HEIGHT);
  uint8_t* pa = framebuffer + (HEIGHT+1)*WIDTH;
  uint8_t* pb = framebuffer + (HEIGHT+HEIGHT-2)*WIDTH;
  for(int x = 0; x < WIDTH; ++x) { *pb++ = *pa++ = 0x13; }
  for(unsigned int n = 0; n < lines.size(); ++n) {
    auto& line = lines[n];
    int y = n + TOP_PADDING;
    memcpy(framebuffer + (HEIGHT+y)*WIDTH + LEFT_PADDING,
           line.data(), line.size());
  }
  ModalErrorDelegate del;
  display.SetInputDelegate(&del);
  display.Update(WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, framebuffer);
  while(!del.DidExit()) display.Pump(true);
  display.SetInputDelegate(NULL);
}
