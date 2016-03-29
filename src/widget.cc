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

#include "tttp_common.h"

using namespace Widgets;

Widget::Widget(Container& container, int x, int y, int w, int h)
  : focused(false), container(container), x(x), y(y), w(w), h(h) {}
Widget::~Widget() {}

void Widget::Draw() { container.DirtyRect(x, y, w, h); }
void Widget::OnGainFocus() { focused = true; Draw(); }
void Widget::OnLoseFocus() { focused = false; Draw(); }
void Widget::HandleText(const uint8_t* text, size_t textlen) {
  (void)text; (void)textlen;
}
void Widget::HandleKey(tttp_scancode scancode) {
  std::shared_ptr<Widget> w = GetMyself();
  switch(scancode) {
  case KEY_LEFT:
    do { w = w->GetLeftNeighbor(); } while(w && !w->IsEnabled());
    if(w) container.SetFocusedWidget(w);
    break;
  case KEY_RIGHT:
    do { w = w->GetRightNeighbor(); } while(w && !w->IsEnabled());
    if(w) container.SetFocusedWidget(w);
    break;
  case KEY_UP:
    do { w = w->GetUpNeighbor(); } while(w && !w->IsEnabled());
    if(w) container.SetFocusedWidget(w);
    break;
  case KEY_DOWN:
    do { w = w->GetDownNeighbor(); } while(w && !w->IsEnabled());
    if(w) container.SetFocusedWidget(w);
    break;
  case KEY_TAB:
    if(container.IsShiftHeld())
      do { w = w->GetShiftTabNeighbor(); } while(w && !w->IsEnabled());
    else
      do { w = w->GetTabNeighbor(); } while(w && !w->IsEnabled());
    if(w) container.SetFocusedWidget(w);
    break;
  default:
    container.UnhandledKey(scancode);
    break;
  }
}
void Widget::HandleClick(uint16_t button) {
  if(button == TTTP_LEFT_MOUSE_BUTTON) container.SetFocusedWidget(GetMyself());
}
bool Widget::IsEnabled() const { return false; }

std::shared_ptr<Widget> Widget::GetMyself() const {
  auto ret = myself.lock();
  if(ret == nullptr)
    throw std::string("We weren't added to a Container before GetMyself() was"
                      " called");
  return ret;
}

std::shared_ptr<Widget> Widget::GetTabNeighbor() const {
  return tab_neighbor.lock();
}

std::shared_ptr<Widget> Widget::GetShiftTabNeighbor() const {
  return shift_tab_neighbor.lock();
}

std::shared_ptr<Widget> Widget::GetUpNeighbor() const {
  auto ret = up_neighbor.lock();
  if(ret != nullptr) return ret;
  else return GetShiftTabNeighbor();
}

std::shared_ptr<Widget> Widget::GetDownNeighbor() const {
  auto ret = down_neighbor.lock();
  if(ret != nullptr) return ret;
  else return GetTabNeighbor();
}

std::shared_ptr<Widget> Widget::GetLeftNeighbor() const {
  auto ret = left_neighbor.lock();
  return ret;
}

std::shared_ptr<Widget> Widget::GetRightNeighbor() const {
  auto ret = right_neighbor.lock();
  return ret;
}

std::shared_ptr<Widget> Widget::SetUpNeighbor(std::weak_ptr<Widget> nu) {
  auto hard = nu.lock();
  if(hard) { up_neighbor = nu; return hard; }
  else return GetMyself();
}
std::shared_ptr<Widget> Widget::SetDownNeighbor(std::weak_ptr<Widget> nu) {
  auto hard = nu.lock();
  if(hard) { down_neighbor = nu; return hard; }
  else return GetMyself();
}
std::shared_ptr<Widget> Widget::SetLeftNeighbor(std::weak_ptr<Widget> nu) {
  auto hard = nu.lock();
  if(hard) { left_neighbor = nu; return hard; }
  else return GetMyself();
}
std::shared_ptr<Widget> Widget::SetRightNeighbor(std::weak_ptr<Widget> nu) {
  auto hard = nu.lock();
  if(hard) { right_neighbor = nu; return hard; }
  else return GetMyself();
}
std::shared_ptr<Widget> Widget::SetTabNeighbor(std::weak_ptr<Widget> nu) {
  auto hard = nu.lock();
  if(hard) {
    tab_neighbor = nu;
    hard->shift_tab_neighbor = GetMyself();
    return hard;
  }
  else return GetMyself();
}
