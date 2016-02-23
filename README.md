A client for the TTTP protocol. Supports all aspects of the current version of the protocol, including authentication, encryption, and pasting. Fairly command-line oriented and not very thoroughly tested... pasting support and "file-based passwords" are not tested at all. Should work on Windows (good luck building it) and most Unices. Build not tested on Mac OS X but the Cocoa code in TEG _is_ tested, so...

A complete list of packages needed to build on Debian Jessie:

- g++
- gcc
- libgmp-dev
- libpng12-dev
- libsdl2-dev
- libsqlite3-dev
- lua5.2
- make
