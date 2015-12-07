#include "display.hh"

#include <iostream>

Display::Display() : delegate(NULL) {}

Display::~Display() {}

void Display::SetInputDelegate(InputDelegate* delegate) {
  this->delegate = delegate;
}

void Display::Statusf(const char* format, ...) {
  char buf[MAX_STATUS_LINE_LENGTH+1];
  va_list arg;
  va_start(arg, format);
  vsnprintf(buf, sizeof(buf), format, arg);
  va_end(arg);
  std::string nu(buf);
  if(nu != status) {
    status = std::move(nu);
    StatusChanged();
  }
  if(delegate == nullptr) {
    // nobody is listening, force an update
    DiscardingInputDelegate del;
    delegate = &del;
    Pump();
    delegate = nullptr;
  }
}

void DiscardingInputDelegate::Key(int, tttp_scancode) {}
void DiscardingInputDelegate::Text(uint8_t*, size_t) {}
void DiscardingInputDelegate::MouseMove(int16_t, int16_t) {}
void DiscardingInputDelegate::MouseButton(int, uint16_t) {}
void DiscardingInputDelegate::Scroll(int8_t, int8_t) {}

char* Display::GetOtherClipboardText() { return nullptr; }
void Display::FreeOtherClipboardText(char*) {}
