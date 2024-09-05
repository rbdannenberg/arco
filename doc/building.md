# Building an Arco Application

**Roger B. Dannenberg**


## Introduction

This page is under construction...

Options: server, link with Serpent, link with another application

Arco is built as a library, but to use it, of course you need some
kind of application. Currently, two applications exist:

- A server that uses the curses library for a simple interface in
  a terminal. You can run the server and connect to it with O2
  from anything. The server is a full O2 server, so the
  application can use O2lite, which also means it could be
  running in a web browser. (Some slight changes are needed to
  enable HTTP in the server.)

- wxSerpent is a real-time python-like scripting language with
  the ability to create graphical interfaces using wxWidgets.
  Arco can run in a wxSerpent process using O2's shared memory
  interface.

These applications are in `apps/basic` (the server) and
`apps/test` (creates wxSerpent + Arco).

The intent is that you can copy one of these apps subdirectories to
make a custom version of Arco with a terminal interface or as part of
wxSerpent. The main reason to make a new application is to add a
custom selection of unit generators.

## Dependencies

You need:
- A C++ compiler, linker, etc.
- CMake for building a project or makefile
- FAUST developer installation
- For Fluidsynth (Arco's `flsyn` unit generator) you need:
  - Fluidsynth, e.g. install and [build from github sources](
    https://github.com/FluidSynth/fluidsynth/releases/tag/v2.3.3).
    You also need to define FLSYN_INCL, FLSYN_OPT_LIB and
    FLSYN_DBG_LIB in `libraries.txt` (see below).
  - Glib 2.0, e.g. you can install with Homebrew or package managers
    on Linux. See `libraries.txt` below, which must be configured to
    reference your Glib library file.) You need to define GLIB_OPT_LIB
    and GLIB_DBG_LIB in `libraries.txt` (see below).
  - `intl`, e.g. you can install with Homebrew or package managers
    on Linux (and it may come with Glib 2.0). You need to define
    INTL_OPT_LIB and INTL_DBG_LIB in `libraries.txt` (see below).

## Defining a New Ugen Using FAUST
Arco implements an automated toolchain allowing unit generators to
be specified and (mostly) compiled by FAUST. While FAUST unit
generators have fairly rigid interfaces (once compiled), Arco is
flexible in the number of channels and types of inputs, which may
be audio rate (a-rate, meaning BL audio samples per block, where BL
is an Arco compile-time constant normally set to 32), block
rate (b-rate, or one sample per block running at the audio rate / BL),
or constant (c-rate, or float values that can be changed through Arco
messages).

To accomodate Arco's unit generator design, Arco uses a translator
that decomposes FAUST-generated code (in C++) and reconstructs a new
implementation (also in C++) for Arco. Normally, it is necessary to
run the FAUST compiler multiple times to create code for each combination
of a-rate and b-rate inputs, so there is a code generation system
that prepares input files for FAUST and runs FAUST's compiler based
on a descriptor file.

To summarize, you need a descriptor file that contains not only a
FAUST implementation of your unit generator but also some meta-data
about input and output types and options. This file (with extension
`.ugen`) is read by the Arco toolchain. Ultimately, a `.h` and a
`.cpp` file are produced with a complete Arco unit generator
implementation (or if you want both a-rate and b-rate output versions,
you will get two `.h` files and two `.cpp` files). These are linked
into your application by adding the unit generator name to your
application's `dspmanifest.txt` file.

### Defining a .ugen file
We will now walk through an example that creates two basic unit
generators for Arco: `mult` (a-rate output) and `multb` (b-rate
output). `mult` multiplies two inputs to form the output. It can
have multiple channels, and the output is audio rate if you
make an instance of `mult` and block rate if you make an instance
of `multb` (the `b` suffix is a standard notation used throughout
Arco to designate b-rate unit generator variants).

Here is `mult.ugen` followed by a description:
```
# mult.ugen -- mult unit generator
#
# Roger B. Dannenberg
# Jan 2022

mult(ab, ab): a
multb(b, b): b

FAUST

declare description "Mult(iply) Unit Generator for Arco";
declare interpolated "x1 x2";

import("stdfaust.lib");

process(x1, x2) = x1 * x2;
```
Note that comments can be inserted following hash characters.

The first part of the description is meta-data on types. This says
that `mult` takes two parameters which may be a-rate or b-rate
(indicated by `ab`) and produces an a-rate output (indicated by
`: a` after the parameters).

The next line indicates that there is another unit generator named
`multb` that produces a b-rate output. Notice the input parameters
for this are restricted to b-rate (indicated by `b` instead of `ab`).

The line containing `FAUST` means that everything that follows is
a FAUST program. See FAUST documentation for details.

The `description` declaration is purely for documentation and is
ignored by Arco processing.

If you want block-rate parameters to be interpolated to audio rate,
you must declare `interpolated` as shown. Parameters are in a single
string and separated by spaces.

Typical FAUST programs might not be one-liners like 
`process(x1, x2) = x1 * x2;` and there is no requirement to make such
simple audio processors.

This `mult.ugen` file must be placed in a directory named
`arco/ugens/mult/`. `CMake` (or Linux's `ccmake`) will find all the
directories in `arco/ugens/` and insure that the Arco toolchain is used
to create all the unit generators defined there.

Often, when you are developing new unit generators, you will have
compile-time errors and want to examine the FAUST compiler output.
This will appear in CMake's output, but is more easily viewed as
normal terminal output. You can process this or any other unit generator
using
```
cd arco/ugens/mult
python3 ../../preproc/u2f.py sine
```
to create FAUST files from `mult.ugen`. This step precedes actually
running FAUST. If everything looks good after this step, you can run
```
sh generate_mult.sh
```
This will run FAUST's compiler on the generated FAUST source files.
You can of course break this down into smaller steps by manually
running FAUST on a single file if you wish. See the commands in
`generate_mult.sh` and the `run_faust` function in
`../../preporc/f2a.py` to see shell commands for these sub-steps.

## CMake and `libraries.txt`

`apps/common/libraries.txt` tells CMake where to find libraries that
are needed to build Arco and applications. This is not a standard
use of CMake, but (in my opinion), trying to find libraries
automatically with CMake in a host-independent manner is too
fragile. It is really a hack trying to overcome the failure of
operating systems and development environment to make libraries
findable. After much frustration writing more and more code simply to
locate the libraries we need, I turned to the bone-simple approach of
including libraries.txt which simply sets a bunch of CMake variables
to full paths to the libraries. You just have to find your library
once and record its location. We do not want to just set the location
in the CMake cache because someday you'll start fresh or clear the
cache and lose all the information, so it is better to edit
libraries.txt than cache variables (although they are there and you
can edit them in CMake for temporary changes if you wish.)

If you do not find `apps/common/libraries.txt`, copy
`apps/common/libraries-example.txt` to `apps/common/libraries.txt` and
edit it. Git excludes `apps/common/libraries.txt` from the repo so
that developers do not create conflicts by customizing it to their
machine(s).

The variables FLSYN_INCL, FLSYN_OPT_LIB, FLSYN_DBG_LIB, GLIB_OPT_LIB,
GLIB_DBG_LIB, INTL_OPT_LIB and INTL_DBG_LIB are only required for
Fluidsynth (unit generator name: `flsyn`) and can be omitted if you
do not use `flsyn`.

## List Ugens: `dspmanifest.txt`

To customize the set of unit generators in Arco, copy the whole app
subdirectory to a new one and edit `dspmanifest.txt`. We keep sources
for all Arco unit generators (for now) in the git repo, but only
include the unit generators listed in `dspmanifest.txt`. This allows
you to build small servers or libraries for embedded systems (other
applications or even microcomputers) with only the code you need. It's
a bit of a pain to rebuild Arco just because now you want to use the
grainwacker unit generator in your patch, so the plan is to put a
rather large set into `apps/test` and `apps/basic`, so you can
*remove* things you do not need, and maybe later we'll have a cool way
to build the `dspmanifest.txt` automatically from declarations in
source files.

## Running CMake
Once dspmanifest.txt is in place, you can run CMake in/on your
`apps/test` directory, and if all goes well, you can generate a
project for XCode, Visual Studio, or a Makefile for Linux.

If there are problems, you may see a lot of text in CMake's output. If
you are trying to debug FAUST code and your `.ugen` files, you may
prefer to run make in a terminal window. The section below on
**Details** gives some options.

## Making a Server

## Linking with wxSerpent

## Details: The Preprocessor Pipeline

- `dspmanifest.txt` tells the build system what unit generators you want

- `CMake` calls `makedspmakefile.py` to build a makefile:

- `makedspmakefile.py` reads `dspmanifest.txt` and creates:
  - `dspmakefile` (input to make to generate the unit generator
    sources). *Notes:*
    - Once `CMake` has created this, you can just run
    `make --makefile=dspmakefile` in your `apps/project` directory to
    rerun FAUST. This is recommended if you are debugging
    FAUST code because the output appears in the terminal instead of
    in CMake's output window.

    - Perhaps even better, you can make a particular unit generator
      with ``make --makefile=dspmakefile <path-to-a-ugen>/<a-ugen>.cpp`
      in your `apps/project` directory to remake one unit generator.

  - `dspsources.cmakeinclude` (tells the project to compile and link the
    unit generator sources)

  - `allugens.srp` (A concatenation of all the Serpent classes and constructor
    functions to be loaded by client programs that use the set of Ugens
    listed in dspmanifest.txt. This saves typing in a large set of `require`
    statements to load each Ugen separately, or alternatively, it having and
    loading a "master" interface file with *all* interface functions,
    including ones for unused Ugens.)

- `CMake` invokes `make --makefile=dspmakefile` which invokes `u2f.py` and
  `generate_<ugenname>.sh`

- `u2f.py` reads a `.ugen` file describing variants of the unit generator
  based on what sample rates (audio-rate or block-rate) can be handled
  as inputs and what output rates are to be produced. It creates:

  - multiple `.dsp` files for FAUST

  - `generate_<ugenname>.sh` (scripts immediately invoked by `dspmakefile` )

- `generate_<ugenname>.sh` runs `f2a.py` and `o2idc.py` to generate audio-rate and block-rate versions unit generators for Arco.

- `f2a.py` reads multiple `.cpp` and `.h` files written by FAUST and creates:
  - `.cpp` and `.h` files that become Arco sources.

- `o2idc.py` is O2's interface description compiler. It reads some comments
  from the `.cpp` and `.h` files and expands them into code to extract O2
  message parameters into local variables in message-handling functions.
  It also writes code to register the functions as handlers for O2 messages.

In general, two unit generators are
created for each basic DSP algorithm. For example, `sine.ugen` directs
the build system to create the Arco unit generators `Sine` and `Sineb`
which produce audio-rate and block-rate signals, respectively.
