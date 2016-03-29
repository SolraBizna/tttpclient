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

void Widgets::ModalInfo(Display& display, const std::string& prompt,
                        uint8_t color) {
  std::vector<std::string> lines = break_lines(prompt, 72);
  int height = lines.size() + 6;
  Container container(display, 80, height);
  bool finished = false;
  std::shared_ptr<LooseText> text
    = std::make_shared<LooseText>(container, 4, 2, 72, lines.size(),
                                  lines, color);
  container.AddWidget(text);
  std::shared_ptr<Button> ok_button
    = std::make_shared<Button>(container, 56, height-3, 20,
                               "OK", [&finished]() {
                                 finished = true;
                               });
  container.AddWidget(ok_button);
  container.FocusFirstEnabledWidget();
  container.SetKeyHandler([&finished](tttp_scancode key) -> bool {
      switch(key) {
      case KEY_KEYPAD_ENTER:
      case KEY_ENTER:
        finished = true;
        break;
      default: break;
      }
      return false;
    });
  container.RunModal([&finished]() -> bool { return finished; });
}
