#ifndef PKDBHH
#define PKDBHH

#include "tttpclient.hh"
#include "tttp_common.h"

class Display;

namespace PKDB {
  // false: the database could not be initialized, the connection can only
  // proceed without PKDB (so with authentication and encryption disabled)
  // may use the passed Display to prompt the user for necessary, potentially
  // dangerous steps
  bool Init(Display&, std::string& whynot);
  // false: there is no public key on file for this canonical name
  bool GetPublicKey(const std::string& canon_name,
                    uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH]);
  // calls die if the public key could not be changed
  void ChangePublicKey(const std::string& canon_name,
                       const uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH]);
  // calls die if the public key could not be added, does not check for
  // dupes
  void AddPublicKey(const std::string& canon_name,
                    const uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH]);
  // calls die if removing all public keys for that canon name failed, does NOT
  // fail if there were no keys!
  void WipePublicKey(const std::string& canon_name);
  // call when the connection is established and PKDB is no longer needed
  void Fini();
}

#endif
