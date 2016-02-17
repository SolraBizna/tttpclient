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

#include "startup.hh"
#include "widgets.hh"
#include "modal_error.hh"
#include "mac16.hh"
#include "tttp_common.h"
#include "charconv.hh"

#include <algorithm>
#include <regex>
#include <lsx.h>

static std::forward_list<Net::Address> connection_targets;

static std::string port_to_string(uint16_t port) {
  // std::to_string is missing on MinGW, and it makes me sad
  char buf[6];
  snprintf(buf, 6, "%hu", port);
  return buf;
}

static bool extract_port(std::string& error_out,
                         const std::string& src,
                         std::string& out_host, uint16_t& out_port) {
  auto colon_count = std::count(src.cbegin(), src.cend(), ':');
  if(colon_count > 7) {
    error_out = "The address had too many colons to be anything valid. If you want to enter an IPv6 address with a port number, enclose the IPv6 address in [].\n\nExample: [::1]:3000";
    return false;
  }
  else if(colon_count == 0) {
    // it's probably a bare address, use the standard port
    out_host = src;
    out_port = TTTP_STANDARD_PORT;
    return true;
  }
  static std::regex nonv6_regex("^([^:]+):([0-9]{1,5})$");
  static std::regex v6_regex("^\\[([0-9A-Fa-f:]+)\\]:([0-9]{1,5})$");
  std::smatch result;
  if(!std::regex_match(src, result, v6_regex,
                       std::regex_constants::match_continuous)) {
    if(colon_count > 1) {
      // the only way for this to make sense is if it's a bare IPv6 address
      static std::regex bare_v6_regex("^[0-9A-Fa-f:]+$");
      if(!std::regex_match(src, bare_v6_regex,
                           std::regex_constants::match_continuous)) {
        error_out = "The address had a lot of colons in it but didn't look like a bare IPv6 address.";
        return false;
      }
      // it's close enough to looking like a bare IPv6 address, let's try it
      out_host = src;
      out_port = TTTP_STANDARD_PORT;
      return true;
    }
    else if(!std::regex_match(src, result, nonv6_regex,
                              std::regex_constants::match_continuous)) {
      error_out = "We couldn't make sense of the address.";
      return false;
    }
  }
  // result[2] contains the port
  std::string port = result[2];
  char* endptr;
  errno = 0;
  long l = strtol(port.c_str(), &endptr, 10);
  if(errno != 0 || *endptr || endptr == port.c_str() || l < 1 || l > 65535) {
    error_out = "The address specified a port outside the range of valid ports. Port numbers must range from 1 to 65535, inclusive. (Preferably, they should range from 1024 to 49151, inclusive.)";
    return false;
  }
  // result[1] contains the hostname portion
  out_host = result[1];
  out_port = (uint16_t)l;
  return true;
}

static std::string get_canon_name(const std::string& host,
                                  uint16_t port) {
  if(port == TTTP_STANDARD_PORT)
    return host;
  else if(std::count(host.cbegin(), host.cend(), ':') > 0)
    return "[" + host + "]:" + port_to_string(port);
  else
    return host + ":" + port_to_string(port);
}

static bool get_canon_name(std::string& error_out,
                           const std::string& source,
                           std::string& out_canon_name) {
  std::string host; uint16_t port;
  if(!extract_port(error_out, source, host, port)) return false;
  out_canon_name = get_canon_name(host, port);
  return true;
}

class NonLoopback {
public:
  bool operator()(const Net::Address& addr) { return !addr.IsLoopback(); }
};

static bool resolve(Display& display, std::string& error_out,
                    const std::string& entered_address,
                    std::string& canon_name_out) {
  std::string host;
  uint16_t port;
  if(!extract_port(error_out, entered_address, host, port)) return false;
  canon_name_out = get_canon_name(host, port);
  display.Statusf("%s", "Looking up host...");
  connection_targets.clear();
  bool ret = Net::ResolveHost(error_out, connection_targets, host.c_str(),
                              port);
  display.Statusf("");
  if(ret && connection_targets.cbegin() == connection_targets.cend()) {
    ret = false;
    error_out = "There are no addresses associated with that name. At least, none that we can try to connect to from this machine.";
  }
  if(ret) {
    // If the first address returned is for the loopback address, we will only
    // try to connect to loopback. This simplifies certain things, especially
    // where the "non-encrypted non-local connection" warning is concerned.
    if(connection_targets.cbegin()->IsLoopback())
      connection_targets.remove_if(NonLoopback());
  }
  return ret;
}

static const std::string SCARY_WARNING = "WARNING: You have disabled"
  " encryption on this connection, but it does not look like a connection to"
  " localhost. Unless this is a connection over a secured VPN you should not"
  " continue.\n\nA connection over an actual LAN, or TOR, should not be"
  " considered secure!\n\nAre you sure you want to continue?";

static bool valid_hostname_char(uint8_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
    || (c >= '0' && c <= '9') || c == '-' || c == ':' || c == '.'
    || c == '[' || c == ']';
}

bool DoConnectionDialog(Display& display) {
  Widgets::Container container(display, 80, 9);
  std::string connection_user, connection_pass, canon_name;
  if(autohost) {
    std::string err;
    const char* p = autohost;
    while(*p) {
      if(!valid_hostname_char(*p++)) {
        Widgets::ModalInfo(display, std::string("The address entered on the"
                                                " command line could not be"
                                                " used.\n\n")
                           + "It contained invalid characters.",
                           MAC16_BLACK|(MAC16_RED<<4));
        return false;
      }
    }
    container.DrawAll();
    DiscardingInputDelegate del;
    display.SetInputDelegate(&del);
    container.Update();
    display.SetInputDelegate(nullptr);
    if(!resolve(display, err, autohost, canon_name)) {
      Widgets::ModalInfo(display, std::string("The address entered on the"
                                              " command line could not be"
                                              " used.\n\n") + err,
                         MAC16_BLACK|(MAC16_RED<<4));
      return false;
    }
  }
  if(autohost && no_crypt
     && !connection_targets.cbegin()->IsLoopback()
     && !Widgets::ModalConfirm(display, SCARY_WARNING))
    return false;
  if(autohost && ((autouser && (autopassword||autopassfile))
                  || no_auth)) {
    if(!no_auth) {
      connection_user = autouser;
      if(autopassword) connection_pass = autopassword;
    }
    return AttemptConnection(display,
                             autohost,
                             connection_targets,
                             autouser ? autouser : "",
                             autopassword
                             ? reinterpret_cast<const uint8_t*>(autopassword)
                             : nullptr,
                             autopassword ? strlen(autopassword) : 0,
                             no_crypt);
  }
  else {
    std::shared_ptr<Widgets::LabeledField> host_widget
      (new Widgets::LabeledField(container, 4, 2, 72, 14,
                                 "Server Address", autohost ? autohost :
                                 "localhost", valid_hostname_char));
    if(autohost) host_widget->SetIsEnabled(false);
    container.AddWidget(host_widget);
    std::shared_ptr<Widgets::Widget> user_widget;
    std::shared_ptr<Widgets::Widget> pass_widget;
    std::shared_ptr<Widgets::LabeledField> username_widget = nullptr;
    std::shared_ptr<Widgets::SecureLabeledField> password_widget = nullptr;
    if(no_auth) {
      user_widget
        = std::make_shared<Widgets::LooseText>(container, 4, 3, 72, 2,
                                               "WARNING: Authentication has"
                                               " been disabled on the command"
                                               " line! Authentication AND"
                                               " encryption are disabled!",
                                               (MAC16_YELLOW<<4)
                                               |MAC16_BLACK);
      pass_widget = std::make_shared<Widgets::Widget>(container, 0, 0, 0, 0);
    }
    else {
      username_widget
        = std::make_shared<Widgets::LabeledField>
        (container, 4, 3, 72, 14,
         "Username", autouser ? autouser : "",
         [](uint8_t c) -> bool {
          return c >= 0x20 && c <= 0x7E;
        }, 255);
      if(autouser) username_widget->SetIsEnabled(false);
      user_widget = username_widget;
      if(autopassfile)
        pass_widget = std::make_shared<Widgets::LooseText>
          (container, 4, 4, 72, 1, "Password file given on command line");
      else {
        password_widget = std::make_shared<Widgets::SecureLabeledField>
          (container, 4, 4, 72, 14, "Password", autopassword);
        pass_widget = password_widget;
        if(autopassword) password_widget->SetIsEnabled(false);
      }
    }
    container.AddWidget(user_widget);
    container.AddWidget(pass_widget);
    bool cancelled = false, connecting = false, connecting_insecure = false;
    std::shared_ptr<Widgets::Button> cancel_button
      = std::make_shared<Widgets::Button>(container, 4, 6, 20,
                                          "Cancel", [&cancelled]() {
                                            cancelled = true;
                                          });
    std::shared_ptr<Widgets::Button> connect_no_crypt_button
      = std::make_shared<Widgets::Button>(container, 34, 6, 20,
                                          "Connect (insecure)",
                                          [&connecting,&connecting_insecure](){
                                            connecting = true;
                                            connecting_insecure = true;
                                          });
    std::shared_ptr<Widgets::Button> connect_button
      = std::make_shared<Widgets::Button>(container, 56, 6, 20,
                                          "Connect (secure)", [&connecting]() {
                                            connecting = true;
                                          });
    if(no_crypt) connect_button->SetIsEnabled(false);
    if(!no_auth) {
      std::function<void(bool)> ctrl_handler = [&](bool on){
        if(on) {
          connect_button->SetLabel("Manage key");
          connect_button->SetAction(
            [&display,&host_widget,&container]() {
              std::string canon_name; // also error_out @_@
              if(!get_canon_name(canon_name, host_widget->GetContent(),
                                 canon_name))
                Widgets::ModalInfo(display, std::string("The address you entered cannot be used.\n\n")+canon_name, MAC16_BLACK|(MAC16_ORANGE<<4));
              else
                KeyManageDialog(display,canon_name);
              container.UnNest();
            });
          connect_button->SetIsEnabled(true);
          connect_button->Draw();
        }
        else {
          connect_button->SetLabel("Connect (secure)");
          connect_button->SetAction([&connecting]() {
              connecting = true;
            });
          connect_button->SetIsEnabled(!no_crypt);
          connect_button->Draw();
        }
      };
      container.SetControlKeyStateHandler(ctrl_handler);
    }
    container.AddWidget(cancel_button);
    container.AddWidget(connect_no_crypt_button);
    container.AddWidget(connect_button);
    host_widget->SetTabNeighbor(user_widget)->SetTabNeighbor(pass_widget)
      ->SetTabNeighbor(connect_no_crypt_button)
      ->SetTabNeighbor(connect_button)->SetTabNeighbor(cancel_button)
      ->SetTabNeighbor(host_widget);
    connect_no_crypt_button->SetUpNeighbor(pass_widget);
    connect_button->SetUpNeighbor(pass_widget);
    cancel_button->SetDownNeighbor(host_widget);
    connect_no_crypt_button->SetDownNeighbor(host_widget);
    connect_button->SetDownNeighbor(host_widget);
    cancel_button->SetRightNeighbor(connect_no_crypt_button)
      ->SetRightNeighbor(connect_button)->SetRightNeighbor(cancel_button)
      ->SetLeftNeighbor(connect_button)
      ->SetLeftNeighbor(connect_no_crypt_button)
      ->SetLeftNeighbor(cancel_button);
    if(!no_crypt) {
      pass_widget->SetDownNeighbor(connect_button);
      host_widget->SetUpNeighbor(connect_button);
    }
    else {
      pass_widget->SetDownNeighbor(connect_no_crypt_button);
      host_widget->SetUpNeighbor(connect_no_crypt_button);
    }
    container.FocusFirstEnabledWidget();
    std::function<void()> next_action;
    if(!no_crypt) next_action = [&connect_button]{
        connect_button->HandleClick(TTTP_LEFT_MOUSE_BUTTON);
      };
    else next_action = [&connect_no_crypt_button]{
        connect_no_crypt_button->HandleClick(TTTP_LEFT_MOUSE_BUTTON);
      };
    if(password_widget && password_widget->IsEnabled()) {
      password_widget->SetAction(next_action);
      next_action = [&container,&password_widget]{
        container.SetFocusedWidget(password_widget);
      };
    }
    if(username_widget && username_widget->IsEnabled()) {
      username_widget->SetAction(next_action);
      next_action = [&container,&username_widget]{
        container.SetFocusedWidget(username_widget);
      };
    }
    if(host_widget && host_widget->IsEnabled()) {
      host_widget->SetAction(next_action);
      next_action = [&container,&host_widget]{
        container.SetFocusedWidget(host_widget);
      };
    }
    do {
      connecting = false; connecting_insecure = false; cancelled = false;
      container.RunModal([&connecting,&cancelled]() -> bool
                         { return connecting || cancelled; });
      if(connecting) {
        std::string err;
        if(!autohost && !resolve(display, err, host_widget->GetContent(),
                                 canon_name)) {
          Widgets::ModalInfo(display, std::string("The address you entered"
                                                  " could not be used.\n\n")
                             + err);
          continue;
        }
        else if(connecting_insecure
                && !no_crypt // they would already have been warned once
                && !connection_targets.cbegin()->IsLoopback()
                && !Widgets::ModalConfirm(display, SCARY_WARNING))
          continue;
        else {
          const uint8_t* pp; size_t pl;
          if(password_widget) password_widget->GetContent(pp, pl);
          else pl = 0;
          uint8_t* pp_utf8 = reinterpret_cast<uint8_t*>(safe_malloc(pl*3));
          size_t pp_utf8_len = convert_cp437_to_utf8(pp, pp_utf8, pl)-pp_utf8;
          bool ret = AttemptConnection(display,
                                       canon_name,
                                       connection_targets,
                                       username_widget
                                       ? username_widget->GetContent() : "",
                                       pp, pl,
                                       connecting_insecure);
          lsx_explicit_bzero(pp_utf8, pp_utf8_len);
          safe_free(pp_utf8);
          return ret;
        }
      }
    } while(!cancelled);
  }
  return false;
}
