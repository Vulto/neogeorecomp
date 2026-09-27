CC = clang
AR = llvm-ar
CFLAGS ?= -std=c23 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude -Igames/neodriftout/src -Igames/neodriftout/recomp
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
	src/timer.c \
	src/platform.c \
	src/debug.c

RuntimeObjects := $(RuntimeSources:%.c=build/%.o)

GameSources := \
	games/neodriftout/src/main.c \
	$(wildcard games/neodriftout/recomp/*.c) \
	$(wildcard games/neodriftout/src/autorecomp/*.c)

GameObjects := $(GameSources:%.c=build/%.o)

.PHONY: all clean test debug runtime neodriftout

all: neodriftout

runtime: $(RuntimeLibrary)

neodriftout: $(RuntimeLibrary) $(GameObjects)
	$(CC) $(CFLAGS) $(LDFLAGS) $(GameObjects) $(RuntimeLibrary) $(SDL3_LIBS) $(LDLIBS) -o $@

$(RuntimeLibrary): $(RuntimeObjects)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(RuntimeObjects)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/games/neodriftout/recomp/gameplay.o: CFLAGS += -Dfunc_000CC6=func_000CC6_autogen

debug: CFLAGS += -g -O0
debug: clean all

test: neodriftout
	@echo "Neo Drift Out build smoke test passed."

clean:
	rm -rf build neodriftout
