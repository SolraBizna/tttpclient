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

#ifndef WIDGETSHH
#define WIDGETSHH

#include "tttpclient.hh"
#include "tttp_scancodes.h"
#include "mac16.hh"
#include "display.hh"

#include <vector>
#include <memory>

#define BACKGROUND_COLOR MAC16_BLACK
#define DEFAULT_LOOSE_TEXT_COLOR ((MAC16_GRAY<<4)|MAC16_BLACK)
#define DISABLED_LABEL_COLOR ((MAC16_DARK_GRAY<<4)|MAC16_BLACK)
#define UNSELECTED_LABEL_COLOR ((MAC16_GRAY<<4)|MAC16_BLACK)
#define SELECTED_LABEL_COLOR ((MAC16_LIGHT_GRAY<<4)|MAC16_BLACK)
#define DISABLED_FIELD_COLOR ((MAC16_DARK_GRAY<<4)|MAC16_BLACK)
#define UNSELECTED_FIELD_COLOR ((MAC16_LIGHT_GRAY<<4)|MAC16_BLACK)
#define SELECTED_FIELD_COLOR ((MAC16_WHITE<<4)|MAC16_BLACK)
#define DISABLED_BUTTON_COLOR (MAC16_DARK_GRAY|(MAC16_BLACK<<4))
#define UNSELECTED_BUTTON_COLOR (MAC16_GRAY|(MAC16_BLACK<<4))
#define SELECTED_BUTTON_COLOR (MAC16_LIGHT_GRAY|(MAC16_BLACK<<4))
#define CLICKED_BUTTON_COLOR (MAC16_WHITE|(MAC16_BLACK<<4))

namespace Widgets {

class Container;

class Widget {
  friend class Container;
  std::weak_ptr<Widget> up_neighbor, down_neighbor,
    left_neighbor, right_neighbor, tab_neighbor, shift_tab_neighbor;
  std::weak_ptr<Widget> myself; // only valid after added to a Container
  bool focused;
protected:
  Container& container;
  const int x, y, w, h;
  std::shared_ptr<Widget> GetMyself() const;
public:
  Widget(Container& container, int x, int y, int w, int h);
  inline bool IsFocused() const { return focused; }
  virtual ~Widget();
  virtual void Draw(); // default behavior: call container.DirtyRect
  virtual void OnGainFocus(); // default behavior: update `focused` call `Draw`
  virtual void OnLoseFocus(); // default behavior: update `focused` call `Draw`
  virtual void HandleText(const uint8_t* text, size_t textlen);
  virtual void HandleKey(tttp_scancode scancode); // default: refocus
  virtual void HandleClick(uint16_t button); // default: focus me on left click
  /* if IsEnabled() returns false, HandleClick will not be called, and focus
     will "pass through" this widget
     Default is to always return false */
  virtual bool IsEnabled() const;
  /* these functions obey the substitution rules given below */
  std::shared_ptr<Widget> GetUpNeighbor() const;
  std::shared_ptr<Widget> GetDownNeighbor() const;
  std::shared_ptr<Widget> GetLeftNeighbor() const;
  std::shared_ptr<Widget> GetRightNeighbor() const;
  std::shared_ptr<Widget> GetTabNeighbor() const;
  std::shared_ptr<Widget> GetShiftTabNeighbor() const;
  /* returns a pointer to the widget in question
     for relationships other than tab, this does *not* assume the relationship
     is transitive, unlike Everdeep; you must explicitly go both ways!
     note: if a widget has tab relationships but not up/down ones, it will
     fall back on shift tab/tab, respectively
     because of the above, it is frequently enough to do a single chain of
     `SetTabNeighbor`s
     special case: if the neighbor widget in question is nullptr, does nothing
     and returns a pointer to *this* widget */
  std::shared_ptr<Widget> SetUpNeighbor(std::weak_ptr<Widget> neighbor);
  std::shared_ptr<Widget> SetDownNeighbor(std::weak_ptr<Widget> neighbor);
  std::shared_ptr<Widget> SetLeftNeighbor(std::weak_ptr<Widget> neighbor);
  std::shared_ptr<Widget> SetRightNeighbor(std::weak_ptr<Widget> neighbor);
  std::shared_ptr<Widget> SetTabNeighbor(std::weak_ptr<Widget> neighbor);
};

class Container {
  friend class Widget;
  std::vector<std::shared_ptr<Widget> > widgets;
  std::weak_ptr<Widget> focus_widget;
  bool left_shift_held, right_shift_held;
  bool left_control_held, right_control_held;
  // we consider this equivalent to Control, to smooth things over with Mac
  // users
  bool left_gui_held, right_gui_held;
  Display& display;
  int width, height, mousex, mousey,
    dirty_left, dirty_top, dirty_right, dirty_bot;
  uint8_t* framebuffer, *glyphbuffer; // glyphbuffer points inside framebuffer
  class InputDelegate : public ::InputDelegate {
    Container& container;
  public:
    InputDelegate(Container& container);
    void Key(int pressed, tttp_scancode scancode) override;
    void Text(uint8_t* text, size_t textlen) override;
    void MouseMove(int16_t x, int16_t y) override;
    void MouseButton(int pressed, uint16_t button) override;
    void Scroll(int8_t x, int8_t y) override;
  } delegate;
  std::function<bool(tttp_scancode)> key_handler;
  std::function<void(bool)> control_key_state_handler;
  void UnhandledKey(tttp_scancode);
public:
  inline void DirtyRect(int x, int y, int w, int h) {
    DirtyRegion(x, y, x+w-1, y+h-1);
  }
  inline void DirtyRegion(int l, int t, int r, int b) {
    if(l < dirty_left) dirty_left = l;
    if(t < dirty_top) dirty_top = t;
    if(r > dirty_right) dirty_right = r;
    if(b > dirty_bot) dirty_bot = b;
  }
  Container(Display& display, uint16_t width, uint16_t height);
  ~Container();
  void AddWidget(std::shared_ptr<Widget> widget);
  void SetFocusedWidget(std::shared_ptr<Widget> widget);
  void FocusFirstEnabledWidget();
  // handler should return true if it wishes to override the default
  // "unhandled" behavior for the key
  void SetKeyHandler(std::function<bool(tttp_scancode)> handler);
  // handler will be called when the return value of `IsControlHeld` changes
  void SetControlKeyStateHandler(std::function<void(bool)> handler);
  void DrawAll(); // also clears the screen
  void Update(); // call this before every display.Pump() call
  void RunModal(std::function<bool()> completion_condition,
                bool already_called_draw_all = false);
  // call this if you had someone else in charge of the Display during a call
  // to RunModal
  void UnNest();
  inline bool IsShiftHeld() const { return left_shift_held||right_shift_held; }
  inline bool IsControlHeld() const {
    return left_control_held||right_control_held
      ||left_gui_held||right_gui_held;
  }
  inline uint8_t* GetColorPointer(int x = 0, int y = 0) {
    return framebuffer + width*y + x;
  }
  inline uint8_t* GetGlyphPointer(int x = 0, int y = 0) {
    return glyphbuffer + width*y + x;
  }
  inline Display& GetDisplay() const { return display; }
};

class LooseText : public Widget {
  std::vector<std::string> lines;
  int color;
public:
  LooseText(Container& container, int x, int y, int w, int h,
            const std::string& text, int color = DEFAULT_LOOSE_TEXT_COLOR);
  LooseText(Container& container, int x, int y, int w, int h,
            const std::vector<std::string> lines,
            int color = DEFAULT_LOOSE_TEXT_COLOR);
  ~LooseText() override;
  void SetText(const std::string& text);
  void Draw() override;
};

class LabeledField : public Widget {
  std::string label, content;
  std::function<bool(uint8_t)> filter;
  std::function<void()> action;
  size_t maxlen, cursor_pos;
  unsigned int label_w, text_x, text_w;
  bool enabled;
  void HandlePaste();
public:
  LabeledField(Container& container, int x, int y, int w, int label_w,
               const std::string& label, const std::string& contents = "",
               std::function<bool(uint8_t)> filter = nullptr,
               size_t maxlen = 0);
  ~LabeledField() override;
  const std::string& GetContent() const { return content; }
  void Draw() override;
  void SetIsEnabled(bool enabled);
  bool IsEnabled() const override;
  void HandleText(const uint8_t* text, size_t textlen) override;
  void HandleKey(tttp_scancode scancode) override;
  void HandleClick(uint16_t button) override;
  inline void SetAction(std::function<void()> action) { this->action = action;}
};

class SecureLabeledField : public Widget {
  std::string label;
  class secure_string {
    friend class SecureLabeledField;
    uint8_t* buf;
    size_t bufsize, len;
  public:
    secure_string(const char*);
    ~secure_string();
    inline size_t length() const { return len; }
    void reserve(size_t headroom);
    void insert(size_t i, size_t count, uint8_t c);
    void erase(size_t i, size_t count);
  } content;
  std::function<void()> action;
  size_t cursor_pos;
  unsigned int label_w, text_x, text_w;
  bool enabled;
  void HandlePaste();
public:
  SecureLabeledField(Container& container, int x, int y, int w, int label_w,
                     const std::string& label, const char* contents=nullptr);
  ~SecureLabeledField() override;
  void GetContent(const uint8_t*& outptr, size_t& outlen) const;
  void Draw() override;
  void SetIsEnabled(bool enabled);
  bool IsEnabled() const override;
  void HandleText(const uint8_t* text, size_t textlen) override;
  void HandleKey(tttp_scancode scancode) override;
  void HandleClick(uint16_t button) override;
  inline void SetAction(std::function<void()> action) { this->action = action;}
};

class Button : public Widget {
  std::string label;
  std::function<void()> action;
  bool enabled, clicked;
  int text_offset;
public:
  Button(Container& container, int x, int y, int w,
         const std::string& label, std::function<void()> action = nullptr);
  ~Button();
  void SetAction(std::function<void()> action);
  void SetLabel(const std::string& label);
  void Draw() override;
  void SetIsEnabled(bool enabled);
  bool IsEnabled() const override;
  void HandleKey(tttp_scancode scancode) override;
  void HandleClick(uint16_t button) override;
};

bool ModalConfirm(Display& display, const std::string& prompt);
void ModalInfo(Display& display, const std::string& prompt,
               uint8_t color = DEFAULT_LOOSE_TEXT_COLOR);

}

#endif
