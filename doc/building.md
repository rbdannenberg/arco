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

These applicaitons are in `apps/basic` (the server) and
`apps/test` (creates wxSerpent + Arco).

The intent is that you can copy one of these apps subdirectories to
make a custom version of Arco with a terminal interface or as part of
wxSerpent. The main reason to make a new application is to add a
custom selection of unit generators.

## Defining a New Ugen Using FAUST

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
