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

#include "tttp_common.h"

using namespace Widgets;

Container::InputDelegate::InputDelegate(Container& container)
  : container(container) {}
void Container::InputDelegate::Key(int pressed, tttp_scancode scancode) {
  if(scancode == KEY_LEFT_SHIFT) container.left_shift_held = !!pressed;
  if(scancode == KEY_RIGHT_SHIFT) container.right_shift_held = !!pressed;
  if(scancode == KEY_LEFT_CONTROL || scancode == KEY_RIGHT_CONTROL
     || scancode == KEY_LEFT_GUI || scancode == KEY_RIGHT_GUI) {
    bool old_held = container.IsControlHeld();
    if(scancode == KEY_LEFT_CONTROL) container.left_control_held = !!pressed;
    if(scancode == KEY_RIGHT_CONTROL) container.right_control_held = !!pressed;
    if(scancode == KEY_LEFT_GUI) container.left_gui_held = !!pressed;
    if(scancode == KEY_RIGHT_GUI) container.right_gui_held = !!pressed;
    bool new_held = container.IsControlHeld();
    if(new_held != old_held && container.control_key_state_handler)
      container.control_key_state_handler(new_held);
  }
  if(!pressed) return;
  auto p = container.focus_widget.lock();
  if(p) p->HandleKey(scancode);
  else container.UnhandledKey(scancode);
}
void Container::InputDelegate::Text(uint8_t* text, size_t textlen) {
  auto p = container.focus_widget.lock();
  if(p) p->HandleText(text, textlen);
}
void Container::InputDelegate::MouseMove(int16_t x, int16_t y) {
  container.mousex = x; container.mousey = y;
}
void Container::InputDelegate::MouseButton(int pressed, uint16_t button) {
  if(!pressed) return;
  if(button == TTTP_MIDDLE_MOUSE_BUTTON) {
    /* make middle-click-to-paste behave consistently with an emulated
       terminal */
    auto p = container.focus_widget.lock();
    if(p) p->HandleClick(button);
  }
  else {
    for(auto& widget : container.widgets) {
      if(widget->IsEnabled() && container.mousex >= widget->x
         && container.mousey >= widget->y
         && container.mousex < widget->x+widget->w
         && container.mousey < widget->y+widget->h)
        widget->HandleClick(button);
    }
  }
}
void Container::InputDelegate::Scroll(int8_t, int8_t) {}

Container::Container(Display& display, uint16_t width, uint16_t height)
  : left_shift_held(false), right_shift_held(false),
    left_control_held(false), right_control_held(false),
    left_gui_held(false), right_gui_held(false),
    display(display), width(width), height(height), mousex(-1), mousey(-1),
    dirty_left(0), dirty_top(0), dirty_right(width-1), dirty_bot(height-1),
    framebuffer(NULL), glyphbuffer(NULL), delegate(*this) {
  if(width*2*height/height/2 != width) throw std::string("Integer overflow");
  framebuffer = (uint8_t*)safe_calloc(width*2, height);
  glyphbuffer = framebuffer + width*height;
}
Container::~Container() {
  safe_free(framebuffer);
}

void Container::AddWidget(std::shared_ptr<Widget> widget) {
  if(!widget) return;
  widget->myself = widget;
  widgets.push_back(widget);
}

void Container::SetFocusedWidget(std::shared_ptr<Widget> widget) {
  auto focused = focus_widget.lock();
  if(!focused || focused != widget) {
    if(focused) focused->OnLoseFocus();
    focus_widget = widget;
    if(widget) widget->OnGainFocus();
  }
}

void Container::FocusFirstEnabledWidget() {
  for(auto& widget : widgets) {
    if(widget->IsEnabled()) { SetFocusedWidget(widget); break; }
  }
}

void Container::SetKeyHandler(std::function<bool(tttp_scancode)> handler) {
  this->key_handler = handler;
}

void Container::SetControlKeyStateHandler(std::function<void(bool)> handler) {
  this->control_key_state_handler = handler;
}

void Container::UnhandledKey(tttp_scancode code) {
  if(key_handler) if(key_handler(code)) return;
  switch(code) {
  case KEY_TAB: case KEY_LEFT: case KEY_RIGHT: case KEY_UP: case KEY_DOWN:
    if(!focus_widget.lock()) FocusFirstEnabledWidget();
    break;
  default: break;
  }
}

void Container::DrawAll() {
  memset(framebuffer, BACKGROUND_COLOR, width*height);
  memset(glyphbuffer, ' ', width*height);
  for(auto& w : widgets) w->Draw();
  dirty_left = 0; dirty_top = 0;
  dirty_right = width-1; dirty_bot = height-1;
}

void Container::Update() {
  if(dirty_left > dirty_right || dirty_top > dirty_bot) return;
  display.Update(width, height, dirty_left, dirty_top,
                 dirty_right-dirty_left+1, dirty_bot-dirty_top+1,
                 framebuffer);
  dirty_left = width; dirty_top = height;
  dirty_right = 0; dirty_bot = 0;
}

void Container::RunModal(std::function<bool()> completion_condition,
                         bool already_called_draw_all) {
  display.SetInputDelegate(&delegate);
  while(!completion_condition()) {
    if(!already_called_draw_all) {
      DrawAll();
      already_called_draw_all = true;
    }
    Update();
    display.Pump(true);
  }
  display.SetInputDelegate(nullptr);
}

void Container::UnNest() {
  display.SetInputDelegate(&delegate);
  DrawAll();
  right_shift_held = left_shift_held = false;
  if(left_control_held || right_control_held) {
    right_control_held = left_control_held = false;
    if(control_key_state_handler)
      control_key_state_handler(false);
  }
}
