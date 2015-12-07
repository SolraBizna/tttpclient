#include "widgets.hh"
#include "tttp_common.h"

using namespace Widgets;

Button::Button(Container& container, int x, int y, int w,
               const std::string& label, std::function<void()> action)
  : Widget(container, x, y, w, 1),
    label(label), action(action), enabled(true), clicked(false),
    text_offset((w-label.size())/2) {}
Button::~Button() {}

void Button::SetAction(std::function<void()> action) {
  this->action = action;
}

void Button::SetLabel(const std::string& label) {
  this->label = label;
  text_offset = (w-label.size())/2;
}

void Button::Draw() {
  memset(container.GetColorPointer(x,y),
         clicked ? CLICKED_BUTTON_COLOR :
         IsEnabled() ? IsFocused() ? SELECTED_BUTTON_COLOR
         : UNSELECTED_BUTTON_COLOR : DISABLED_BUTTON_COLOR, w);
  memset(container.GetGlyphPointer(x,y), ' ', w);
  memcpy(container.GetGlyphPointer(x,y)+text_offset,
         label.data(), label.size());
  Widget::Draw();
}

void Button::SetIsEnabled(bool enabled) {
  this->enabled = enabled;
}

bool Button::IsEnabled() const {
  return enabled && action;
}

void Button::HandleKey(tttp_scancode scancode) {
  switch(scancode) {
  case KEY_SPACE:
    clicked = true;
    Draw();
    container.Update();
    SDL_Delay(100);
    if(action) action();
    clicked = false;
    Draw();
    // do not update again
    break;
  default: Widget::HandleKey(scancode);
  }
}

void Button::HandleClick(uint16_t button) {
  assert(IsEnabled());
  if(button == TTTP_LEFT_MOUSE_BUTTON) {
    clicked = true;
    Draw();
    container.Update();
    SDL_Delay(100);
    if(action) action();
    clicked = false;
    Draw();
    // do not update again
  }
  else Widget::HandleClick(button);
}
