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
      case KEY_ENTER:
        finished = true;
        break;
      default: break;
      }
      return false;
    });
  container.RunModal([&finished]() -> bool { return finished; });
}
