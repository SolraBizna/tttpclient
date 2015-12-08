#include "startup.hh"
#include "widgets.hh"
#include "pkdb.hh"

void KeyManageDialog(Display& display,
                     const std::string& canon_name) {
  bool have_key = false;
  uint8_t key[TTTP_PUBLIC_KEY_LENGTH];
  uint8_t filekey[TTTP_PUBLIC_KEY_LENGTH];
  char fingerbuf[TTTP_FINGERPRINT_BUFFER_SIZE];
  Widgets::Container container(display, 80, 16);
  bool finished = false;
  std::shared_ptr<Widgets::LabeledField> host_widget
    (new Widgets::LabeledField(container, 4, 2, 72, 14,
                               "Server Address", canon_name));
  host_widget->SetIsEnabled(false);
  container.AddWidget(host_widget);
  std::shared_ptr<Widgets::LooseText> filed_widget;
  container.AddWidget(std::make_shared<Widgets::LooseText>
                      (container, 4, 4, 72, 2, "Press " PMOD "-C to copy the"
                       " existing public key (if there is one).\nPress"
                       " " PMOD "-V to paste a new public key."));
  container.AddWidget(std::make_shared<Widgets::LooseText>
                      (container, 4, 7, 72, 1, "Fingerprint of public key on"
                       " file for this server:"));
  bool have_filekey = PKDB::GetPublicKey(canon_name, filekey);
  if(have_filekey) {
    tttp_get_key_fingerprint(filekey, fingerbuf);
    filed_widget = std::make_shared<Widgets::LooseText>
      (container, 4, 8, 72, 1, std::string("  ")+fingerbuf);
  }
  else {
    filed_widget = std::make_shared<Widgets::LooseText>
      (container, 4, 8, 72, 1, "  (no public key on file)");
  }
  container.AddWidget(filed_widget);
  container.AddWidget(std::make_shared<Widgets::LooseText>
                      (container, 4, 10, 72, 1, "Fingerprint of new public key:"));
  std::shared_ptr<Widgets::LooseText> candidate_widget
    (new Widgets::LooseText(container, 4, 11, 72, 1, "  (no key given)"));
  std::shared_ptr<Widgets::Button> change_button;
  auto update_candidate = [&]() {
    if(have_key) {
      tttp_get_key_fingerprint(key, fingerbuf);
      if(have_filekey && !memcmp(key, filekey, TTTP_KEY_LENGTH)) {
        candidate_widget->SetText(std::string("  ")+fingerbuf+" (same key)");
        have_key = false;
      }
      else
        candidate_widget->SetText(std::string("  ")+fingerbuf);
      candidate_widget->Draw();
    }
    else {
      candidate_widget->SetText("  (no key given)");
      candidate_widget->Draw();
    }
    change_button->SetIsEnabled(have_key);
    change_button->Draw();
  };
  container.AddWidget(candidate_widget);
  auto copy = [&display,&filekey]() {
    char buf[TTTP_KEY_BASE64_BUFFER_SIZE+14
             +TTTP_FINGERPRINT_BUFFER_SIZE+1];
    tttp_key_to_base64(filekey, buf);
    memcpy(buf+TTTP_KEY_BASE64_BUFFER_SIZE-1,
           "\n\nFingerprint: ", 15);
    tttp_get_key_fingerprint(filekey, buf+TTTP_KEY_BASE64_BUFFER_SIZE+14);
    buf[sizeof(buf)-2] = '\n';
    buf[sizeof(buf)-1] = 0;
    display.SetClipboardText(buf);
  };
  auto paste = [&display,&update_candidate,&have_key,&key]() {
    have_key = false;
    char* clipboard_text = display.GetClipboardText();
    if(clipboard_text) {
      have_key = tttp_key_from_base64(clipboard_text,
                                      key);
      if(have_key && tttp_key_is_null_public_key(key))
        have_key = false;
      display.FreeClipboardText(clipboard_text);
    }
    if(!have_key) {
      clipboard_text = display.GetOtherClipboardText();
      if(clipboard_text) {
        have_key = tttp_key_from_base64(clipboard_text,
                                        key);
        if(have_key && tttp_key_is_null_public_key(key))
          have_key = false;
        display.FreeOtherClipboardText(clipboard_text);
      }
    }
    update_candidate();
  };
  std::shared_ptr<Widgets::Button> cancel_button
    (new Widgets::Button(container, 4, 13, 20, "Cancel",
                         [&finished]() { finished = true; }));
  container.AddWidget(cancel_button);
  std::shared_ptr<Widgets::Button> clear_button
    (new Widgets::Button(container, 34, 13, 20, "Clear",
                         [&]() {
                           if(Widgets::ModalConfirm(display,
                                                    "Are you sure you want to"
                                                    " delete this key from the"
                                                    " database?")) {
                             PKDB::WipePublicKey(canon_name);
                             finished = true;
                           }
                           container.UnNest();
                         }));
  container.AddWidget(clear_button);
  clear_button->SetIsEnabled(have_filekey);
  change_button = std::make_shared<Widgets::Button>
    (container, 56, 13, 20, have_filekey?"Change":"Add",
     [&finished,&have_key,&have_filekey,&key,&canon_name]() {
      assert(have_key);
      if(have_filekey) PKDB::ChangePublicKey(canon_name, key);
      else PKDB::AddPublicKey(canon_name, key);
      finished = true;
    });
  change_button->SetIsEnabled(false);
  container.AddWidget(change_button);
  (void)update_candidate;
  cancel_button->SetTabNeighbor(clear_button)->SetTabNeighbor(change_button)
    ->SetTabNeighbor(cancel_button);
  cancel_button->SetRightNeighbor(clear_button)
    ->SetRightNeighbor(change_button)->SetRightNeighbor(cancel_button);
  cancel_button->SetLeftNeighbor(change_button)
    ->SetLeftNeighbor(clear_button)->SetLeftNeighbor(cancel_button);
  container.FocusFirstEnabledWidget();
  container.SetKeyHandler([&finished,&copy,&paste,&container,&have_filekey]
                          (tttp_scancode key) -> bool {
                            switch(key) {
                            case KEY_ESCAPE:
                              finished = true;
                              break;
                            case KEY_C:
                              if(container.IsControlHeld() && have_filekey)
                                copy();
                              break;
                            case KEY_V:
                              if(container.IsControlHeld())
                                paste();
                              break;
                            default: break;
                            }
                            return false;
                          });
  container.DrawAll();
  container.RunModal([&finished]() -> bool { return finished; });
}
