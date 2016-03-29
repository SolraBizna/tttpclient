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

#include "widgets.hh"
#include "break_lines.hh"

using namespace Widgets;

LooseText::LooseText(Container& container, int x, int y, int w, int h,
                     const std::string& text, int color)
  : Widget(container, x, y, w, h), lines(break_lines(text, w)), color(color) {}
LooseText::LooseText(Container& container, int x, int y, int w, int h,
                     const std::vector<std::string> lines, int color)
  : Widget(container, x, y, w, h), lines(lines), color(color) {}
LooseText::~LooseText() {}

void LooseText::SetText(const std::string& text) {
  lines = break_lines(text, w);
}

void LooseText::Draw() {
  int y = this->y;
  for(const auto& line : lines) {
    memset(container.GetColorPointer(x, y),
           color, line.length());
    memset(container.GetGlyphPointer(x, y),
           ' ', line.length());
    memcpy(container.GetGlyphPointer(x, y),
           line.data(), line.length());
    memset(container.GetGlyphPointer(x, y) + line.length(),
           ' ', w-line.length());
    ++y;
  }
  Widget::Draw();
}

