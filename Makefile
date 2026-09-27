CC = clang
CXX = clang++
AR = llvm-ar
CFLAGS ?= -std=c23 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude -Igames/neodriftout/src -Igames/neodriftout/recomp -Ithird_party/z80 -Ithird_party/ymfm/src
SDL3_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LIBS ?= $(shell pkg-config --libs sdl3 2>/dev/null)

CFLAGS += $(SDL3_CFLAGS)

RuntimeLibrary := build/libneogeorecomp.a

RuntimeSources := \
	src/neogeorecomp.c \
	src/m68k.c \
	src/bus.c \
	src/func_table.c \
	src/video.c \
	src/palette.c \
	src/io.c \
	src/ym2610.c \
	src/z80.c \
	third_party/z80/z80.c \
	src/timer.c \
	src/platform.c \
	src/debug.c \
	src/ym2610_backend.cpp \
	third_party/ymfm/src/ymfm_opn.cpp \
	third_party/ymfm/src/ymfm_misc.cpp \
	third_party/ymfm/src/ymfm_ssg.cpp \
	third_party/ymfm/src/ymfm_adpcm.cpp

RuntimeObjects := $(RuntimeSources:%.c=build/%.o)
RuntimeCxxSources := $(filter %.cpp,$(RuntimeSources))
RuntimeCxxObjects := $(RuntimeCxxSources:%.cpp=build/%.o)

GameSources := \
	src/neodriftout_main.c \
	$(wildcard games/neodriftout/recomp/*.c) \
	$(wildcard games/neodriftout/src/autorecomp/*.c)

GameObjects := $(GameSources:%.c=build/%.o)

.PHONY: all clean test debug runtime neodriftout romcheck run

all: neodriftout

runtime: $(RuntimeLibrary)

neodriftout: $(RuntimeLibrary) $(GameObjects)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(LDFLAGS) $(GameObjects) $(RuntimeLibrary) $(SDL3_LIBS) $(LDLIBS) -o $@

$(RuntimeLibrary): $(RuntimeObjects) $(RuntimeCxxObjects)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(RuntimeObjects) $(RuntimeCxxObjects)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/games/neodriftout/recomp/gameplay.o: CFLAGS += -Dfunc_000CC6=func_000CC6_autogen
build/games/neodriftout/recomp/overrides.o: CFLAGS += -Dfunc_01229E=func_01229E_upstream

build/third_party/z80/z80.o: CFLAGS += -Dz80_init=z80_core_init

debug: CFLAGS += -g -O0
debug: clean all

test: neodriftout
	@echo "Neo Drift Out build smoke test passed."

romcheck:
	@test -n "$(ROM_DIR)" || (echo "usage: make romcheck ROM_DIR=/path/to/roms" >&2; exit 2)
	@sh tools/check_neodriftout_roms.sh "$(ROM_DIR)"

run: neodriftout
	@test -n "$(ROM_DIR)" || (echo "usage: make run ROM_DIR=/path/to/roms" >&2; exit 2)
	@sh tools/check_neodriftout_roms.sh "$(ROM_DIR)"
	./neodriftout --rom-path "$(ROM_DIR)"

clean:
	rm -rf build neodriftout
