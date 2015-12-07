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

