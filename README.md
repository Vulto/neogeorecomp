# neogeorecomp

**A static recompilation runtime for SNK Neo Geo MVS/AES games.**

This project provides the hardware abstraction layer and 68000 CPU runtime needed to statically recompile Neo Geo arcade games into native x86-64 executables. Instead of emulating the hardware cycle-by-cycle, we lift the original Motorola 68000 machine code into equivalent C, then compile it natively alongside a runtime that reproduces the Neo Geo's video, audio, and I/O behavior.

The result: your favorite Neo Geo games running as native PC applications — no emulator required.

## How Static Recompilation Works

Traditional emulation interprets each CPU instruction at runtime. Static recompilation takes a different approach:

1. **Disassemble** the game's 68000 P ROM into labeled assembly
2. **Lift** each function into equivalent C code using our instruction macros (`M68K_ADD16`, `M68K_CMP32`, `M68K_BCC`, etc.)
3. **Register** each function at its original address in a dispatch table
4. **Link** against this runtime library, which provides the Neo Geo memory map, video rendering, audio, and input
5. **Compile** everything with a standard C compiler — out comes a native executable

The CPU instruction macros faithfully reproduce the 68000's behavior, including all condition code flag updates (C, V, Z, N, X). The bus layer routes memory accesses through the Neo Geo's memory map, dispatching reads and writes to the appropriate hardware subsystem.

For a deeper dive into the theory and practice of static recompilation, see the [recompclass](https://github.com/sp00nznet/recompclass) educational materials.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Game Binary (P ROM)                   │
│              Motorola 68000 machine code                │
└───────────────────────┬─────────────────────────────────┘
                        │ static recompilation
                        ▼
┌─────────────────────────────────────────────────────────┐
│                  Recompiled C Source                     │
│          M68K_ADD16(d[1], d[0]);  // add.w d0, d1       │
│          if (M68K_CC_EQ) func_table_call(0x1234);       │
└───────────────────────┬─────────────────────────────────┘
                        │ links against
                        ▼
┌─────────────────────────────────────────────────────────┐
│                   neogeorecomp runtime                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────┐  │
│  │  m68k.h  │ │  bus.h   │ │ video.h  │ │ ym2610.h  │  │
│  │ CPU ctx  │ │ mem map  │ │ LSPC/SCB │ │   audio   │  │
│  │ + macros │ │ + I/O    │ │ sprites  │ │  YM2610   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────┘  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────┐  │
│  │palette.h │ │  z80.h   │ │ timer.h  │ │ platform.h│  │
│  │ 2-bank   │ │ audio CPU│ │ IRQ/VBL  │ │   SDL2    │  │
│  │ 4096 col │ │ 4 MHz    │ │ watchdog │ │  window   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────┘  │
└─────────────────────────────────────────────────────────┘
```

## Neo Geo Hardware at a Glance

| Component | Specification |
|-----------|--------------|
| Main CPU | Motorola 68000 @ 12 MHz |
| Audio CPU | Zilog Z80 @ 4 MHz |
| Sound | Yamaha YM2610 (4 FM + 3 SSG + 7 ADPCM channels) |
| Resolution | 320x224 @ ~59.19 Hz |
| Sprites | 381 per frame, 96 per scanline, 16x16 tiles in vertical strips |
| Fix Layer | 40x32 tiles, 8x8 pixels each, always on top |
| Palettes | 256 palettes x 16 colors, 2 banks, 65,536 possible colors |
| Work RAM | 64 KiB (68k) + 2 KiB (Z80) |

The Neo Geo's video system is sprite-based with no background tile layers (unlike the SNES or Genesis). Everything except the fix layer text overlay is drawn with sprites. Sprites are vertical strips — a single sprite is one tile wide and up to 32 tiles tall. Wide objects are built by chaining sprites horizontally via a "sticky bit." Hardware shrinking (no zoom) is supported per-sprite.

## Project Structure

```
neogeorecomp/
├── include/neogeorecomp/
│   ├── neogeorecomp.h     — main header, frame loop, initialization
│   ├── m68k.h             — 68000 CPU context, instruction macros, condition codes
│   ├── bus.h              — Neo Geo memory map, memory read/write routing
│   ├── func_table.h       — function dispatch table for recompiled code
│   ├── video.h            — LSPC sprite engine, fix layer, SCB tables
│   ├── palette.h          — dual-bank palette system, color conversion
│   ├── io.h               — controller input, system registers, DIP switches
│   ├── ym2610.h           — YM2610 sound chip interface
│   ├── z80.h              — Z80 audio CPU, NMI communication, bank switching
│   ├── timer.h            — interrupt system, VBlank, timer, watchdog
│   ├── platform.h         — SDL3 windowing, input mapping, audio output
│   └── debug.h            — tracing, breakpoints, memory inspection
├── src/                   — implementation files matching each header
├── docs/
│   ├── architecture.md    — deep dive into the runtime architecture
│   ├── memory_map.md      — complete Neo Geo 68k memory map reference
│   └── getting_started.md — how to recompile your first Neo Geo game
├── tools/
│   └── disasm/            — 68000 disassembly and analysis helpers
├── Makefile              — Clang/C23/SDL3 build and ROM workflow
└── LICENSE
```

## Building

### Prerequisites

- Clang
- LLVM (llvm-ar)
- GNU Make
- SDL3 development libraries
- Git
- unzip (only needed when preparing a ROM ZIP)

### Build

The repository uses **C23 + Clang + Make + SDL3**. A normal clone is enough: the first make initializes the required Git submodules automatically.

```bash
git clone https://github.com/Vulto/neogeorecomp.git
cd neogeorecomp
make
```

This produces the native neodriftout executable.

### Neo Drift Out: one-command ROM setup and run

ROM data is never committed to the repository. Give make the path to your legally obtained Neo Drift Out ZIP. The workflow extracts only the required files, validates their size and SHA-256, and stores the prepared set under build/roms/neodriftout.

The normal workflow is:

```bash
make run ROM="$HOME/downloads/neodrift.zip"
```

That command automatically:

1. initializes missing submodules;
2. builds neodriftout;
3. extracts the required ROM files from the ZIP;
4. validates every required ROM by size and SHA-256;
5. starts the native executable.

The ZIP may contain BIOS files or other ROMs; they are ignored. The runtime needs these eight files:

```text
000-lo.lo
213-p1.p1
213-s1.s1
213-c1.c1
213-c2.c2
213-m1.m1
213-v1.v1
213-v2.v2
```

### Separate preparation and validation

If you want to prepare the ROM set without launching the game:

```bash
make prepare ROM="$HOME/downloads/neodrift.zip"
```

If the ROMs are already extracted:

```bash
make prepare ROM_DIR=/path/to/neodrift-roms
make run ROM_DIR=/path/to/neodrift-roms
```

The lower-level targets are also available:

```bash
make prepare-rom ROM=/path/to/neodrift.zip
make romcheck
make run
```

By default, prepared ROMs go to build/roms/neodriftout. Override the destination with ROM_DIR=/path/to/output.

The original ZIP and extracted ROM data remain local and are never uploaded by the build or CI.

## Game Projects Using This Runtime

| Game | Developer | Year | Status | Repository |
|------|-----------|------|--------|------------|
| [Metal Slug: Super Vehicle-001](https://github.com/sp00nznet/metalslug) | Nazca Corporation | 1996 | Scaffolding | `sp00nznet/metalslug` |
| [Neo Drift Out: New Technology](https://github.com/sp00nznet/neodriftout) | Visco Corporation | 1996 | **Active native runtime validation** | `sp00nznet/neodriftout` |

### Neo Drift Out Status

The repository contains a native executable target and a deterministic runtime self-test. The self-test does not use game ROM data: it starts SDL3 in headless mode, initializes the runtime, registers the generated function table, and verifies representative function-dispatch, Work RAM, and palette operations.

A successful self-test is a build/runtime sanity check; it is **not** evidence that the game itself boots correctly. Game execution requires the complete validated Neo Drift Out ROM set.

![Neo Drift Out Screenshot](https://raw.githubusercontent.com/sp00nznet/neodriftout/master/docs/screenshot_proof_of_life.png)

## Neo Drift Out decompilation coverage

The game sources are statically recompiled 68000 C, but generated code is not automatically considered complete. The repository exposes an audit target that reports untranslated instructions and operand expressions instead of silently treating them as valid recompilation:

```bash
make decomp-audit
```

The audit currently reports generated `TODO` instructions so missing 68000 translation coverage can be addressed systematically. A clean build or runtime self-test does **not** imply that every generated instruction has been translated.

## Neo Geo Memory Map

The 68000 addresses 16 MB of address space. The runtime's bus layer routes every access:

```
$000000-$0FFFFF  P ROM bank 1 (1 MB, fixed — vectors + main code)
$100000-$10FFFF  Work RAM (64 KB)
$200000-$2FFFFF  P ROM bank 2 (1 MB, bankswitchable for ROMs > 2 MB)
$300000          Player 1 inputs
$300001          DIP switches (read) / Watchdog kick (write)
$320000          Sound command/reply (68k ↔ Z80 via NMI latch)
$340000          Player 2 inputs
$380000          System status (Start, Select, MVS/AES flag, memory card)
$3A00xx          System control (shadow, vector swap, fix/cart select, palette bank, SRAM lock)
$3C0000-$3C000E  LSPC video registers (VRAM address, data, modulo, mode, timer, IRQ ack)
$400000-$401FFF  Palette RAM (8 KB, 2 banks of 256 palettes)
$800000-$BFFFFF  Memory card
$C00000-$C1FFFF  System ROM / BIOS (128 KB)
$D00000-$D0FFFF  Backup RAM (MVS only, battery-backed)
```

## How This Relates to genrecomp

This project shares the same philosophy as [genrecomp](https://github.com/sp00nznet/genrecomp) (our Sega Genesis static recompiler). The 68000 CPU layer — context struct, instruction macros, condition code handling — is architecturally identical. What changes is everything below the CPU:

| Layer | genrecomp (Genesis) | neogeorecomp (Neo Geo) |
|-------|--------------------|-----------------------|
| CPU | M68000 @ 7.67 MHz | M68000 @ 12 MHz |
| Video | VDP (2 scroll planes + sprites) | LSPC (sprites only + fix layer) |
| Audio | YM2612 + SN76489 | YM2610 (FM + SSG + ADPCM) |
| Co-CPU | Z80 @ 3.58 MHz | Z80 @ 4 MHz |
| Memory Map | Genesis-specific | Neo Geo-specific |
| Bus Routing | Via Genesis Plus GX | Custom HAL |

The 68000 is the 68000. Once you can recompile code for one 68k platform, you can recompile for any of them. The hard work is in the hardware abstraction — and that's what this project provides for the Neo Geo.

## Want to Recompile a Different Neo Geo Game?

We designed this runtime to support *any* Neo Geo title. The game-specific work lives in separate repositories (see the table above). To target a new game:

1. Obtain a legal dump of your game's P ROM
2. Disassemble the 68000 code (we recommend [Ghidra](https://ghidra-sre.org/) with its MC68000 processor module)
3. Lift functions into C using our macro layer (see `docs/getting_started.md`)
4. Create a new project that links against `libneogeorecomp`
5. Register your recompiled functions and run

See the Metal Slug and Neo Drift Out projects for examples of how to structure a game-specific recomp.

## Community and Resources

### Neo Geo Development
- [Neo Geo Development Wiki](https://wiki.neogeodev.org) — the definitive hardware reference
- [ngdevkit](https://github.com/dciabrin/ngdevkit) — open-source Neo Geo C development toolkit
- [freemlib](https://github.com/freem/freemlib-neogeo) — Neo Geo assembly utility library
- [Neo Geo Book](https://neogeobook.mattgreer.dev) — memory map and register documentation
- [ChibiAkumas 68000 Neo Geo Tutorial](https://www.chibiakumas.com/68000/neogeo.php) — 68k assembly guide

### Static Recompilation
- [recompclass](https://github.com/sp00nznet/recompclass) — "Static Recompilation: From Theory to Practice" (educational)
- [N64Recomp](https://github.com/N64Recomp/N64Recomp) — the pioneering N64 static recompiler by Wiseguy
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — Xbox 360 PowerPC recompiler
- [genrecomp](https://github.com/sp00nznet/genrecomp) — our Sega Genesis 68000 recompiler (sister project)

### Neo Geo Communities
- [Neo Geo Forever](https://neogeoforever.com) — forums + Discord for Neo Geo enthusiasts and developers
- [Neo-Geo.com Forums](https://www.neo-geo.com/forums/) — long-running community hub
- [Arcade-Projects](https://www.arcade-projects.com) — arcade hardware hacking and homebrew
- [neogeodev GitHub org](https://github.com/neogeodev) — hardware RE, schematics, and tools

### MAME References
- [MAME Neo Geo driver](https://github.com/mamedev/mame/blob/master/src/mame/neogeo/neogeo.cpp) — the canonical hardware reference implementation
- [MAME Neo Geo video](https://github.com/mamedev/mame/blob/master/src/mame/neogeo/neogeo_v.cpp) — sprite and fix layer rendering
- [MAME Dev Wiki: Neo Geo](https://wiki.mamedev.org/index.php/Driver:NeoGeo) — driver documentation

### Reverse Engineering Tools
- [Ghidra](https://ghidra-sre.org/) — NSA's open-source RE framework (supports MC68000)
- [IDANeoGeo](https://github.com/neogeodev/IDANeoGeo) — IDA Pro loader for Neo Geo binaries
- [m68k-disasm](https://github.com/Oxore/m68k-disasm) — standalone 68000 disassembler

## License

MIT — see [LICENSE](LICENSE) for details.

## Contributing

This is an active project and we welcome contributions. Whether you're interested in:
- Improving the hardware abstraction accuracy
- Adding support for advanced cartridge features (NEO-CMC, NEO-PVC encryption)
- Optimizing the sprite renderer
- Tackling a new game recomp
- Writing documentation or tutorials

Open an issue or PR. Let's make Neo Geo games run natively everywhere.
