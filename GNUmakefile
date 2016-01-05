include make/cur_target.mk

CPPFLAGS+=-DGAME_PRETTY_NAME="\"TTTP Client\""
CPPFLAGS+=-DXGL_ENABLE_VBO -DXGL_ENABLE_RECTANGLE_TEXTURES -DXGL_ENABLE_SRGB
CPPFLAGS+=-DXGL_ENABLE_SHADERS -DXGL_ENABLE_FBO -DNO_LUA=1
CPPFLAGS+=-DTEG_NO_DIE_IMPLEMENTATION -DTEG_NO_POSTINIT
CPPFLAGS+=-DTTTP_CLIENT_VERSION="\"v1.0b3\""

EXE_LIST=tttpclient paint

TEG_OBJECTS=obj/teg/io.o obj/teg/xgl.o obj/teg/main.o obj/teg/miscutil.o obj/teg/netsock.o

all: dirs gen \
	$(addsuffix -debug$(EXE),$(addprefix bin/,$(EXE_LIST))) \
	$(addsuffix -release$(EXE),$(addprefix bin/,$(EXE_LIST)))

dirs:
	@mkdir -p bin obj/teg lib include/gen

gen: include/gen/blend_table.hh include/gen/char_table.hh
# include/gen/fshader_main_body.h include/gen/vshader_main_body.h

make/cur_target.mk:
	@echo Please point cur_target.mk to an appropriate target definition.
	@false

bin/%-release$(EXE): lib/libteg.a obj/%.o
	@echo Linking "$@" "(release)"...
	@$(LD) $(LDFLAGS) $(LDFLAGS_RELEASE) -o "$@" $(filter-out lib/libteg.a,$^) lib/libteg.a $(LIBS)
bin/%-debug$(EXE): lib/libteg.debug.a obj/%.debug.o
	@echo Linking "$@" "(debug)"...
	@$(LD) $(LDFLAGS) $(LDFLAGS_DEBUG) -o bin/"$*"-debug$(EXE) $(filter-out lib/libteg.debug.a,$^) lib/libteg.debug.a $(LIBS)

include/gen/%.h: src/gen_%_h.lua
	@echo Generating "$@"...
	@lua $^

include/gen/%.hh: src/gen_%_hh.lua
	@echo Generating "$@"...
	@lua $^

include/gen/shader_%.h: src/glsl2h.lua src/%.glsl
	@echo Generating "$@"...
	@lua $^ $@

obj/%.o: src/%.m
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.m
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -o "$@" "$<"

obj/%.o: src/%.c
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.c
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -o "$@" "$<"

obj/%.o: lsx/src/%.c
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -std=c99 -o "$@" "$<"
obj/%.debug.o: lsx/src/%.c
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -std=c99 -o "$@" "$<"

obj/lsx_bzero.o: lsx/src/lsx_bzero.c
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -O0 -fno-builtin -std=c99 -o "$@" "$<"
obj/lsx_bzero.debug.o: lsx/src/lsx_bzero.c
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -O0 -fno-builtin -std=c99 -o "$@" "$<"

obj/%.o: libtttp/%.c
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -std=c99 -o "$@" "$<"
obj/%.debug.o: libtttp/%.c
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -std=c99 -o "$@" "$<"

obj/%.o: src/%.cc
	@echo Compiling "$<" "(release)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) $(CXXFLAGS) $(CXXFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.cc
	@echo Compiling "$<" "(debug)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) $(CXXFLAGS) $(CXXFLAGS_DEBUG) -o "$@" "$<"

obj/%.o: src/%.cc
	@echo Compiling "$<" "(release)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) $(CXXFLAGS) $(CXXFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.cc
	@echo Compiling "$<" "(debug)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) $(CXXFLAGS) $(CXXFLAGS_DEBUG) -o "$@" "$<"

clean:
	rm -rf bin obj lib include/gen

include src/exes.mk
include src/teg/teg.mk
include $(wildcard obj/*.d)
include $(wildcard obj/teg/*.d)

.PHONY: all clean data gen
