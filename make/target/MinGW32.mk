# Compiler used to compile C code.
CC=gcc
# Compiler used to compile C++ code.
CXX=g++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=g++
# Library archiver used
AR=ar

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-MP -MMD -Iinclude/ -Isrc/teg/ -Ilsx/include -Ilibtttp -D_UNICODE -DUNICODE -DMINGW -D_WIN32_WINNT=0x0501
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=`sdl2-config --cflags` -Wall -Wextra -Werror -Wno-multichar -c
CFLAGS_DEBUG=-ggdb
CFLAGS_RELEASE=
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=gnu++11 -Woverloaded-virtual -Wno-pmf-conversions 
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-mwindows
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=
# Libraries.
LIBS=-lmingw32 -lws2_32 -lSDL2 -lopengl32 -lglu32 -lpng -lz -lgmp /bin/sqlite3.dll
# Flags passed to the library archiver
ARFLAGS=-rsc

# Extension for executables.
EXE=.exe
