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

#ifndef STARTUPHH
#define STARTUPHH

#include "tttpclient.hh"
#include "netsock.hh"
#include "tttp_client.h"

#include <forward_list>

/* We use control and meta interchangeably. This macro specifies which one we
   should emphasize on the current platform. */
#if MACOSX
#define PMOD "command"
#else
#define PMOD "control"
#endif

class Display;

extern char* autohost, *autouser, *autopassword, *autopassfile;
extern bool no_auth, no_crypt;
extern Net::SockStream server_socket;
extern tttp_client* tttp;
extern int queue_depth;

bool DoConnectionDialog(Display& display);
// when this returns true, `tttp` has a tttp_client handle, a connection is
// open, and the handshake has just succeeded
bool AttemptConnection(Display& display,
                       const std::string& canon_name,
                       std::forward_list<Net::Address> targets,
                       const std::string& username,
                       const uint8_t* password, size_t password_len,
                       bool no_crypt);
void KeyManageDialog(Display& display,
                     const std::string& canon_name);

#endif
