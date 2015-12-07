#include "widgets.hh"

#include <string.h>
#include <lsx.h>

using namespace Widgets;

SecureLabeledField::secure_string::secure_string(const char* wat)
  : buf(nullptr), bufsize(0), len(0) {
  if(wat != nullptr) {
    auto watlen = strlen(wat);
    reserve(watlen);
    while(*wat) insert(length(), 1, *wat++);
  }
}

SecureLabeledField::secure_string::~secure_string() {
  if(buf) { lsx_explicit_bzero(buf, bufsize); safe_free(buf); }
}

void SecureLabeledField::secure_string::reserve(size_t headroom) {
  if(len + headroom > bufsize) {
    size_t nusize = bufsize * 3;
    if(nusize < len + headroom) nusize = len + headroom;
    uint8_t* nubuf = reinterpret_cast<uint8_t*>(safe_malloc(nusize));
    if(buf) {
      memcpy(nubuf, buf, len);
      lsx_explicit_bzero(buf, bufsize);
    }
    buf = nubuf;
    bufsize = nusize;
  }
}

void SecureLabeledField::secure_string::insert(size_t i, size_t count,
                                               uint8_t c) {
  reserve(count);
  memmove(buf + i + count, buf + i, count);
  memset(buf + i, c, count);
  len += count;
}

void SecureLabeledField::secure_string::erase(size_t i, size_t count) {
  memmove(buf + i, buf + i + count, len - i - count);
  len -= count;
}

SecureLabeledField::SecureLabeledField(Container& container, int x, int y,
                                       int w, int label_w,
                                       const std::string& label,
                                       const char* contents)
  : Widget(container, x, y, w, 1),
    label(label), content(contents),
    cursor_pos(content.length()), label_w(label_w), text_x(label_w+2),
    text_w(w-label_w-3), enabled(true) {
}

SecureLabeledField::~SecureLabeledField() {}

void SecureLabeledField::Draw() {
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
  memset(g + text_x, 7, text_len);
  if(text_len < text_w)
    memset(g + text_x + text_len, ' ', text_w - text_len);
  Widget::Draw();
}

void SecureLabeledField::SetIsEnabled(bool enabled) { this->enabled = enabled;}

bool SecureLabeledField::IsEnabled() const { return enabled; }

void SecureLabeledField::HandleText(const uint8_t* text, size_t textlen) {
  while(textlen > 0) {
    content.insert(cursor_pos++, 1, *text);
    ++text; --textlen;
  }
  Draw();
}

void SecureLabeledField::HandleKey(tttp_scancode scancode) {
  switch(scancode) {
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

void SecureLabeledField::GetContent(const uint8_t*& outptr, size_t& outlen) const {
  outptr = content.buf;
  outlen = content.len;
}
