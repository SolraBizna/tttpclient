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
