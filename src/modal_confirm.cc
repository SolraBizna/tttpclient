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

#include "widgets.hh"

#include "break_lines.hh"

using namespace Widgets;

bool Widgets::ModalConfirm(Display& display, const std::string& prompt) {
  std::vector<std::string> lines = break_lines(prompt, 72);
  int height = lines.size() + 6;
  Container container(display, 80, height);
  bool confirmed = false;
  bool finished = false;
  std::shared_ptr<LooseText> text
    = std::make_shared<LooseText>(container, 4, 2, 72, lines.size(),
                                  lines, (MAC16_ORANGE<<4)|MAC16_BLACK);
  container.AddWidget(text);
  std::shared_ptr<Button> no_button
    = std::make_shared<Button>(container, 34, height-3, 20,
                               "No", [&confirmed,&finished]() {
                                 confirmed = false;
                                 finished = true;
                               });
  container.AddWidget(no_button);
  std::shared_ptr<Button> yes_button
    = std::make_shared<Button>(container, 56, height-3, 20,
                               "Yes", [&confirmed,&finished]() {
                                 confirmed = true;
                                 finished = true;
                               });
  container.AddWidget(yes_button);
  no_button->SetTabNeighbor(yes_button);
  yes_button->SetTabNeighbor(no_button);
  no_button->SetRightNeighbor(yes_button)->SetRightNeighbor(no_button);
  yes_button->SetLeftNeighbor(no_button)->SetLeftNeighbor(yes_button);
  container.SetFocusedWidget(nullptr);
  container.SetKeyHandler([&confirmed,&finished](tttp_scancode key) -> bool {
      switch(key) {
      case KEY_Y:
        confirmed = true; finished = true;
        break;
      case KEY_N: case KEY_ESCAPE:
        confirmed = false; finished = true;
        break;
      default: break;
      }
      return false;
    });
  container.RunModal([&finished]() -> bool { return finished; });
  return confirmed;
}
