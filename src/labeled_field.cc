#include "widgets.hh"
#include "charconv.hh"
#include "tttp_common.h"

using namespace Widgets;

LabeledField::LabeledField(Container& container, int x, int y, int w,
                           int label_w,
                           const std::string& label,
                           const std::string& contents,
                           std::function<bool(uint8_t)> filter,
                           size_t maxlen)
  : Widget(container, x, y, w, 1),
    label(label), content(contents), filter(filter), maxlen(maxlen),
    cursor_pos(contents.length()), label_w(label_w), text_x(label_w+2),
    text_w(w-label_w-3), enabled(true) {
}

LabeledField::~LabeledField() {}

void LabeledField::Draw() {
  uint8_t* c = container.GetColorPointer(x,y);
  uint8_t* g = container.GetGlyphPointer(x,y);
  int label_color = IsEnabled() ? IsFocused() ? SELECTED_LABEL_COLOR
    : UNSELECTED_LABEL_COLOR : DISABLED_LABEL_COLOR;
  memset(c, label_color, label_w);
  memcpy(g, label.data(), label.length());
  int inner_color = enabled ? IsFocused() ? SELECTED_FIELD_COLOR
    : UNSELECTED_FIELD_COLOR : DISABLED_FIELD_COLOR;
  memset(c+label_w, inner_color, w-label_w);
  if(IsEnabled()) {
    g[label_w+1] = '<';//'[';
    g[w-1] = '>';//']';
  }
  int text_scroll;
  if(!IsFocused())
    text_scroll = content.length() > text_w ? (content.length()-text_w) : 0;
  else if(cursor_pos <= text_w/2)
    text_scroll = 0;
  else if(cursor_pos >= (int)content.length() - text_w/2 + 1) {
    text_scroll = content.length() - text_w + 1;
    if(text_scroll < 0) text_scroll = 0;
  }
  else
    text_scroll = cursor_pos - text_w/2;
  unsigned int cursor_x = cursor_pos - text_scroll;
  if(IsFocused())
    c[text_x + cursor_x] = MAC16_WHITE|(MAC16_BLACK<<4);
  auto text_len = content.length() - text_scroll;
  if(text_len > text_w) text_len = text_w;
  memcpy(g + text_x, content.data() + text_scroll, text_len);
  if(text_len < text_w)
    memset(g + text_x + text_len, ' ', text_w - text_len);
  Widget::Draw();
}

void LabeledField::SetIsEnabled(bool enabled) { this->enabled = enabled; }

bool LabeledField::IsEnabled() const { return enabled; }

void LabeledField::HandleText(const uint8_t* text, size_t textlen) {
  while(textlen > 0 && (maxlen == 0 || content.length() < maxlen)) {
    if(!filter || filter(*text)) {
      content.insert(cursor_pos++, 1, *text);
    }
    ++text; --textlen;
  }
  Draw();
}

void LabeledField::HandleKey(tttp_scancode scancode) {
  switch(scancode) {
  case KEY_V:
    if(container.IsControlHeld()) {
      char* cbt = container.GetDisplay().GetClipboardText();
      if(!cbt) break;
      auto cbtlen = strlen(cbt);
      // overwrite the buffer as we go, it's okay, CP437 is shorter than UTF-8
      uint8_t* cop = convert_utf8_to_cp437(reinterpret_cast<uint8_t*>(cbt),
                                           reinterpret_cast<uint8_t*>(cbt),
                                           cbtlen,
                                           [this](uint8_t* sofar,
                                                   size_t sofarlen,
                                                   tttp_scancode code){
                                             HandleText(sofar, sofarlen);
                                             if(code != KEY_ENTER
                                                && code != KEY_TAB)
                                               HandleKey(code);
                                           });
      auto outlen = cop - reinterpret_cast<uint8_t*>(cbt);
      if(outlen > 0) HandleText(reinterpret_cast<uint8_t*>(cbt), outlen);
      container.GetDisplay().FreeClipboardText(cbt);
    }
    break;
  case KEY_DELETE:
    if(cursor_pos < content.length()) { content.erase(cursor_pos, 1); Draw(); }
    break;
  case KEY_BACKSPACE:
    if(cursor_pos > 0) { content.erase(--cursor_pos, 1); Draw(); }
    break;
  case KEY_LEFT:
    if(cursor_pos > 0) { --cursor_pos; Draw(); }
    break;
  case KEY_RIGHT:
    if(cursor_pos < content.length()) { ++cursor_pos; Draw(); }
    break;
  case KEY_HOME:
    if(cursor_pos > 0) { cursor_pos = 0; Draw(); }
    break;
  case KEY_END:
    if(cursor_pos < content.length()) { cursor_pos = content.length(); Draw();}
    break;
  default:
    Widget::HandleKey(scancode);
  }
}

void LabeledField::HandleClick(uint16_t button) {
  if(button == TTTP_MIDDLE_MOUSE_BUTTON) {
    char* cbt = container.GetDisplay().GetOtherClipboardText();
    if(!cbt) return;
    auto cbtlen = strlen(cbt);
    // overwrite the buffer as we go, it's okay, CP437 is shorter than UTF-8
    uint8_t* cop = convert_utf8_to_cp437(reinterpret_cast<uint8_t*>(cbt),
                                         reinterpret_cast<uint8_t*>(cbt),
                                         cbtlen,
                                         [this](uint8_t* sofar,
                                                size_t sofarlen,
                                                tttp_scancode code){
                                           HandleText(sofar, sofarlen);
                                           if(code != KEY_ENTER
                                              && code != KEY_TAB
                                              && code != KEY_ESCAPE)
                                             HandleKey(code);
                                         });
    auto outlen = cop - reinterpret_cast<uint8_t*>(cbt);
    if(outlen > 0) HandleText(reinterpret_cast<uint8_t*>(cbt), outlen);
    container.GetDisplay().FreeOtherClipboardText(cbt);
  }
  else Widget::HandleClick(button);
}
