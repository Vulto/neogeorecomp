# neogeorecomp

**A static recompilation runtime for SNK Neo Geo MVS/AES games.**

This project provides the hardware abstraction layer and 68000 CPU runtime needed to statically recompile Neo Geo arcade games into native x86-64 executables. Instead of emulating the hardware cycle-by-cycle, we lift the original Motorola 68000 machine code into equivalent C, then compile it natively alongside a runtime that reproduces the Neo Geo's video, audio, and I/O behavior.

## Building

### Prerequisites

- Clang
- LLVM
- GNU Make
- SDL3 development libraries
- Git (for initializing the required submodules)

### Build Steps

The build automatically initializes the Z80, YMFM, and Neo Drift Out submodules when they are missing:

```bash
git clone https://github.com/Vulto/neogeorecomp.git
cd neogeorecomp
make
```

To initialize the submodules explicitly instead:

```bash
git clone --recurse-submodules https://github.com/Vulto/neogeorecomp.git
cd neogeorecomp
make
```

This produces the native `neodriftout` executable.

## Validating a ROM set

The native runtime does not require ROM data to be committed to this repository. To validate a locally dumped Neo Drift Out set before running the executable:

```bash
make romcheck ROM_DIR=/path/to/roms
make run ROM_DIR=/path/to/roms
```

The checker validates the seven game ROM files plus the required 000-lo.lo system ROM by size and SHA-256. The ROM files themselves remain outside Git and CI.
