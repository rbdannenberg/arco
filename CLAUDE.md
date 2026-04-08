# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arco is a dynamic sound synthesis engine written in C++ by Roger B. Dannenberg. It operates as an embedded server (in-process or standalone), communicates via O2 messages, uses FAUST for DSP code generation, and is typically controlled from Serpent (a real-time Python-like language) or any language with O2 bindings. Unit generators (Ugens) are polymorphic, accepting audio-rate (a-rate, 32-sample blocks), block-rate (b-rate, 1 sample per block), or constant (c-rate) inputs.

## Build System

Arco uses CMake (3.24+). Each application in `apps/` has its own `CMakeLists.txt` and `dspmanifest.txt` that determines which unit generators are linked.

### Build steps (e.g., for the test app `daserpent`)

```bash
cd apps/test
cmake .          # or use CMake GUI
# macOS: make / Xcode build
# Windows: nmake or Visual Studio build
```

On macOS, `build_everything_cmd.sh` automates a full build from scratch including dependencies.

### Build flow

```
apps/<app>/CMakeLists.txt
  -> include(../../serpent/arco.cmakeinclude)  OR  include(../../server/arcoserver.cmakeinclude)
    -> arco/CMakeLists.txt (builds arcolib)
      -> calls makedspmakefile.py at configure time
        -> generates dspmakefile + dspsources.cmakeinclude
          -> dspmakefile runs at build time (u2f.py -> faust -> f2a.py per Ugen)
```

### Machine-local configuration

`apps/common/libraries.txt` defines paths to local libraries (PortAudio, libsndfile, O2, FluidSynth, FAUST, etc.). This file is machine-specific and should not be committed. See `apps/common/libraries-example.txt` for a template.

### Key dependencies

O2, PortAudio, PortMidi, FAUST, libsndfile, FluidSynth, wxWidgets (for GUI apps), Serpent.

All dependency repos (o2, portaudio, portmidi, serpent) must be sibling directories with lowercase names so CMake can find them.

## Code Generation Pipeline

The `.ugen` -> C++ pipeline is central to development:

1. **`.ugen` files** (in `ugens/<name>/`) describe a unit generator with FAUST DSP code plus metadata about input/output rates
2. **`u2f.py`** expands `.ugen` files into multiple FAUST `.dsp` files (one per rate combination)
3. **FAUST compiler** compiles `.dsp` to C++
4. **`f2a.py`** transforms FAUST C++ output into Arco-compatible C++ unit generators (`.h` and `.cpp`)
5. **`makedspmakefile.py`** reads `dspmanifest.txt` and generates a makefile (`dspmakefile`) and CMake include (`dspsources.cmakeinclude`)
6. The generated makefile runs at CMake configure time and at build time

To add a new unit generator: create a `.ugen` file in `ugens/<name>/`, then add the name to the app's `dspmanifest.txt`. A `*` suffix in `dspmanifest.txt` means include both a-rate and b-rate variants.

### .ugen file format

`.ugen` files contain Ugen signatures followed by a blank line, then `FAUST`, then FAUST code:

```
sine(freq: ab, amp: ab): a       # a-rate output, freq/amp accept audio or block rate
sineb(freq: b, amp: b): b        # b-rate variant

FAUST
declare description "Sine Unit Generator for Arco";
declare interpolated "amp";       # linear interpolation from b-rate to a-rate
import("stdfaust.lib");
process(freq, amp) = os.osc(freq)*amp;
```

Rate codes in signatures: `a` = audio-rate, `b` = block-rate, `ab` = either, `c` = constant. Prefix with channel count for multi-channel (e.g., `2a` = 2-channel audio).

Key FAUST metadata declarations:
- `declare interpolated "param"` - generates compact linear interpolation (avoids FAUST's 64K delay line)
- `declare terminate "input"` - input can terminate (affects tail_blocks for reverb tails, etc.)

### dspmanifest.txt format

One Ugen name per line. `*` suffix includes both a-rate and b-rate variants. `#` for comments. Math Ugens (mult, add, etc.) are always auto-included.

### f2a.py rate selection

When the exact rate combination isn't available, `f2a.py` implements a best-match heuristic left-to-right across parameters and inserts upsample/downsample adapters automatically.

## Architecture

### Key directories

- **`arco/src/`** - Core engine: audio I/O (`audioio`), base Ugen class (`ugen`), initialization (`arcoinit`), memory management, built-in Ugens
- **`ugens/`** - FAUST-based unit generators, each in its own subdirectory with a `.ugen` file
- **`preproc/`** - Python preprocessing scripts (`u2f.py`, `f2a.py`, `makedspmakefile.py`, `params.py`, `implementation.py`)
- **`serpent/`** - Serpent integration layer (`arco.cmakeinclude`, Serpent source bindings)
- **`server/`** - Standalone server template (`arcoserver.cmakeinclude`) with curses interface
- **`apps/`** - Application targets; each has its own `CMakeLists.txt` and `dspmanifest.txt`
- **`apps/common/`** - Shared CMake macros (`arco_cmake_macros.txt`) and library path config
- **`cmupv/`** - Phase vocoder / pitch shifting library
- **`doc/`** - Design docs (`design.md`, `building.md`, `ugens.md`, `installation.md`, `constraints.md`)

### Core design concepts

- **Block processing**: All audio computed in blocks of 32 samples (compile-time constant `BL`)
- **Polymorphic Ugens**: Each Ugen handles mixed input types (a-rate/b-rate/c-rate) via auto-generated method pointer variants selected at runtime
- **Reference counting**: Ugens use reference counting for memory management; Serpent garbage-collects its references. `ARCO_REF_DEBUG` mode available for tracking reference cycles
- **Integer IDs**: Ugens are identified by integer IDs (not pointers), enabling language-independent control via O2 messages
- **O2 messages**: All control happens via O2 messages (e.g., `/arco/sine/new`, `/arco/open`, `/arco/reset`)

### O2 message protocol

Control service address is set via `/arco/open` and `/arco/reset`. All Ugen operations use `/arco/<classname>/<method>` (e.g., `/arco/sine/new`, `/arco/sine/1/set_freq`). Replies go to the control service (e.g., `/actl/started`, `/actl/reset`).

Startup sequence: `/arco/reset` -> `/actl/reset` -> create Zero/Thru Ugens for I/O -> `/arco/open` -> `/actl/starting` -> `/actl/started`.

### Audio threading model

Single-threaded main loop with O2 polling. Audio runs in a separate PortAudio callback thread. Handshake protocol between threads via state flags defined in `audioio.h` (IDLE -> STARTING -> STARTED -> FIRST -> RUNNING, etc.).

### Application structure

Apps include the core via `include(../../serpent/arco.cmakeinclude)` (for wxSerpent apps) or `include(../../server/arcoserver.cmakeinclude)` (for standalone servers). Each app is essentially wxSerpent or a curses server with a custom set of linked Ugens determined by its `dspmanifest.txt`.

Two primary app types:
- **`apps/test/`** (`daserpent`): wxSerpent + Arco with GUI, full Ugen set
- **`apps/basic/`**: Curses-based standalone server with minimal Ugen set

### Built-in (non-FAUST) Ugens

The `arco/src/` directory contains hand-written Ugens that are always available: thru, zero, zerob, const, upsample, dnsampleb, multx, testtone, plus many others listed in `makedspmakefile.py`'s NONFAUST list (vu, probe, pwl, delay, allpass, mix, fileplay, filerec, feedback, granstream, etc.).

## Preferences system

Three sets of preference variables with different prefixes (defined in `svprefs.cpp` and `audioio`):
- `p_*` - private to prefs module, values from `.arco` prefs file
- `arco_*` - used for interactive preference selection
- `actual_*` - parameters determined after devices were actually opened

## Windows-specific notes

- Cannot build in a path containing spaces
- Uses `nmake` instead of `make` for the generated dspmakefile
- Python 3 must be available as `python` or `python3`
- Uses Visual Studio (2019+)
