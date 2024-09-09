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

mult(x1: ab, x2: ab): a
multb(x1: b, x2: b): b

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

This description also indicates the Arco parameters. See below for
Multichannel Processing.

The line containing `FAUST` means that everything that follows is
a FAUST program. See FAUST documentation for details.

The `description` declaration is purely for documentation and is
ignored by Arco processing.

If you want block-rate parameters to be interpolated to audio rate,
you must declare `interpolated` as shown. Parameters are in a single
string and separated by commas or spaces. The parameter names
correspond to parameters to "process" defined lower in the file.

Note that in mulichannel processing (see the next section), the
Arco Ugen parameter names may not match the "process" parameters
in name or number, so it is important to use the parameter names
declared by "process" and not those of the Arco signature above
the line "FAUST".

Typical FAUST programs might not be one-liners like 
`process(x1, x2) = x1 * x2;` and there is no requirement to make such
simple audio processors. It is *required* that `process` contain all
the parameters in it's parameter list, e.g. `process(x2) = *(x1)`
might work in FAUST but is not valid in a `.ugen` file.

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

## Multichannel Processing in Ugens

Normally, a Ugen can automatically be accept multichannel inputs.
When the Ugen is created, it is created with a fixed output rate
(audio or block rate) and a fixed number of output channels. The
default is to replicate the Ugen for each output channel. In other
words, for N channels, there will be effectively N mono Ugens, but all
are contained in a single Arco Ugen with a single ID. We will call
these the "mono Ugens."

Inputs should have either 1 or N channels. If less than N, only the
first input channel will be used, and it will be replicated for each
of the N "mono Ugens." (It is normal for an input to have only 1
channel. For example, you can provide a 1-channel cutoff frequency to
a stereo filter so that all channels get the same cutoff frequency.
If an input has greater than N channels, only the first N are used.
You can have multiple inputs, each with a different number of
channels. For example, a stereo filter can have a stereo audio input
and a mono cutoff frequency control. You can even provide a mono audio
input and a 2-channel cutoff frequency so the two output channels have
the same signal filtered with different cutoff frequencies.

Many Arco unit generators do not follow these conventions. For
example, `route` can arbitrarily route channels from an input to
output, and `mix` treats mono inputs as a special case.

The simple way to deal with FAUST algorithms is to define a mono
process and let Arco code generation introduce the logic for producing
multichannel output and handling multichannel inputs. An example is
`lowpass`, defined in FAUST as a single channel Ugen, but usable in
Arco for multichannel processing.

However, some FAUST algorithms are designed for mulitple channels and
are not equivalent to a mono process replicated N times to service N
channels. Instead, they expect multiple input channels and process
them differently. An example is a stereo reverb where the left and
right outputs are each a different function of left and right inputs.
For these cases, we create Arco Ugens with a fixed number of output
channels that matches the FAUST implementation. Each input has a
different expected number of channels, again matching the FAUST
implementation. We specify this in the top part of a `.ugen` file. For
example, a reverb with stereo input and output and two mono controls
might be specified by:
```
strev(in: 2a, wetdry: b, gain: b): 2a
```

An integer preceding "a", "b" or "ab" means that the FAUST
implementation expects some integer number of input signals and they
are supplied by a single input to the Arco Ugen. In Arco, an input to
a "2a" input must have 1 or 2 channels. A mono input is expanded to 2
channels, 2 channels are passed in as 2 channels, and if there are
more than 2 input channels, only the first 2 are used.

If any input or output has an integer prefix (even "1"), *all* inputs
and outputs are processed as having a fixed number of channels, so
`strev(in: 2a, wetdry: b, gain:b): 2a` is just short for 
`strev(in: 2a, wetdry: 1b, gain: 1b): 2a`.

If follows that these three specifications are all different:
```
strev(in: 2a, wetdry: b, gain: b): 2a
strev(in: 2a, wetdry: b, gain: b):  a
strev(in:  a, wetdry: b, gain: b):  a
```

The first describes a FAUST process with stereo input and stereo
output, and there are two block-rate control inputs. The second
describes stereo input with mono output (equivalent to `1a`) and two
control inputs. The third describes a mono process, with mono audio
input and two control inputs. At creation time, the Ugen will receive
a `chans` parameter and the Ugen will be replicated to create an
N-channel Ugen (in the Arco scripting language, the default for
`chans`, if not specified, will be the maximum number of channels of
any of the input signals. (On the other hand, the first two versions
will *not* be expandable at runtime, and there will be no `chans`
parameter to replicate the Ugen to produce more output channels.)

In the FAUST specification, each input must be listed as mono and the
control inputs are specified *as if* they are audio rate since all FAUST
signals are audio rate. For the first example,
```
strevb(in: 2a, wetdry: b, gain: b): 2a
```
the FAUST `process` line will be something like:
```
process(left, right, wetdry, gain) = ...
```

Notice that in terms of mono signals, there are 4 inputs, but in terms
of Arco, there are 3 inputs. This is why we have the meta-data at the
top of the `.ugen` file. The specifications there must be consistent
with the number of parameters in `process`. Here, the Arco code
generator will understand that `left` and `right` are channels 0 and 1
of the 2-channel input named `snd`.

In the `process` definition, you might wonder what `wetdry` and `gain`
are doing as if they are audio rate signals when they are actually
block-rate. This is mainly to create a correspondence between the Arco
Ugen meta-data and the Faust implementation. The FAUST code will be
modified by dropping the block-rate parameters so that process will
become `process(left, right)`, and `wetdry` and `gain` will be defined
as controls using FAUST's `nentry`.  This will allow FAUST to generate
the right code, which will then be processed further and integrated
into an Arco Ugen.

## CMAKE and `libraries.txt`

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
