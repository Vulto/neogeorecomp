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

RuntimeCObjects := $(patsubst %.c,build/%.o,$(filter %.c,$(RuntimeSources)))
RuntimeCxxSources := $(filter %.cpp,$(RuntimeSources))
RuntimeCxxObjects := $(patsubst %.cpp,build/%.o,$(RuntimeCxxSources))

GameSources := \
	src/neodriftout_main.c \
	src/neodriftout_palette_override.c \
	src/neodriftout_runtime_overrides.c \
	src/neodriftout_missing_dispatch.c \
	src/neodriftout_gameplay_override.c \
	$(wildcard games/neodriftout/recomp/*.c) \
	$(wildcard games/neodriftout/src/autorecomp/*.c)

GameObjects := $(GameSources:%.c=build/%.o)

.PHONY: all clean test debug runtime neodriftout prepare-rom romcheck run

ifeq ($(SUBMODULES_READY),1)

all: neodriftout

runtime: $(RuntimeLibrary)

neodriftout: $(RuntimeLibrary) $(GameObjects)
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(LDFLAGS) $(GameObjects) $(RuntimeLibrary) $(SDL3_LIBS) $(LDLIBS) -o $@

test: neodriftout
	@SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy NEOGEO_HEADLESS=1 ./neodriftout --self-test

else

.PHONY: all runtime neodriftout test

# Initialize submodules through Git's own recovery path. A stale plain
# directory is not a usable submodule worktree; remove it before cloning.
SUBMODULE_READY_STAMP := .git/submodules-ready

all runtime neodriftout test: $(SUBMODULE_READY_STAMP)
	@$(MAKE) SUBMODULES_READY=1 $@

$(SUBMODULE_READY_STAMP): .gitmodules
	@git submodule sync --recursive
	@git submodule deinit -f --all >/dev/null 2>&1 || true
	@rm -rf games/neodriftout third_party/z80 third_party/ymfm
	@git submodule update --init --recursive
	@mkdir -p $(@D)
	@touch $@

endif

$(RuntimeLibrary): $(RuntimeCObjects) $(RuntimeCxxObjects)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(RuntimeCObjects) $(RuntimeCxxObjects)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/games/neodriftout/recomp/gameplay.o: override CFLAGS += -Dfunc_000CC6=func_000CC6_autogen -Dfunc_000B34=func_000B34_upstream

# The generated recomp header contains a legacy non-UTF-8 dash in a comment.
# Keep strict warnings for project C while scoping this suppression to the
# translation unit that includes that generated header.
build/src/neodriftout_main.o: override CFLAGS += -Wno-invalid-utf8

# The game submodule is generated recompilation output maintained upstream.
# Keep its warnings visible without allowing them to block this repository's
# strict compilation gate.
build/games/neodriftout/%.o: override CFLAGS += -Wno-error=unused-variable

# Auto-generated recompilation C has legacy labels and a non-UTF-8 comment
# produced by the upstream generator. These diagnostics are scoped to the
# generated translation units only.
build/games/neodriftout/src/autorecomp/%.o: override CFLAGS += -Wno-invalid-utf8 -Wno-unused-label -Wno-error=unused-variable
build/games/neodriftout/recomp/overrides.o: override CFLAGS += -Dfunc_01229E=func_01229E_upstream
build/games/neodriftout/src/autorecomp/recomp_010100_012252.o: override CFLAGS += -Dsub_012036=sub_012036_upstream
build/games/neodriftout/src/autorecomp/recomp_010100_012252.o: override CFLAGS += -Wno-invalid-utf8 -Wno-unused-label -Wno-error=unused-variable

build/third_party/z80/z80.o: override CFLAGS += -Dz80_init=z80_core_init

# YMFM is third-party code and intentionally contains no-op virtual hooks
# whose unused parameters trigger -Werror under Clang. Keep -Werror for the
# project while scoping this suppression to the vendored YMFM translation
# units (including our adapter that includes YMFM headers).
build/src/ym2610_backend.o: override CXXFLAGS += -Wno-unused-parameter
build/third_party/ymfm/src/ymfm_opn.o: override CXXFLAGS += -Wno-unused-parameter
build/third_party/ymfm/src/ymfm_misc.o: override CXXFLAGS += -Wno-unused-parameter
build/third_party/ymfm/src/ymfm_ssg.o: override CXXFLAGS += -Wno-unused-parameter
build/third_party/ymfm/src/ymfm_adpcm.o: override CXXFLAGS += -Wno-unused-parameter

debug: CFLAGS += -g -O0
debug: clean all

ROM ?=
ROM_DIR ?= build/roms/neodriftout

prepare-rom:
	@test -n "$(ROM)" || (echo "usage: make prepare-rom ROM=/path/to/neodrift.zip [ROM_DIR=build/roms/neodriftout]" >&2; exit 2)
	@sh tools/prepare_neodriftout_roms.sh "$(ROM)" "$(ROM_DIR)"

romcheck:
	@if [ -n "$(ROM)" ]; then $(MAKE) --no-print-directory prepare-rom ROM="$(ROM)" ROM_DIR="$(ROM_DIR)"; fi
	@sh tools/check_neodriftout_roms.sh "$(ROM_DIR)"

run: neodriftout
	@$(MAKE) --no-print-directory romcheck ROM="$(ROM)" ROM_DIR="$(ROM_DIR)"
	./neodriftout --rom-path "$(ROM_DIR)"

clean:
	rm -rf build neodriftout
