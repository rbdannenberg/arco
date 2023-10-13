# Arco - sound synthesis engine and framework

**Roger B. Dannenberg**

STATUS: Basic framework is complete, simple demo now runs, needs
much more work to define and build out a library of unit generators.

## Introduction
Arco is a sound synthesis engine that offers dynamic patching of
of unit generators. It is designed to operate as a small embedded
server, running either in its own process or within an application.
In the spirit of "smaller is better," Arco DSP is mainly delegated
to FAUST, leveraging a wide range of existing algorithms. Arco
control is delegated to external languages connecting through O2,
which supports running Arco within the same application, in a
separate process or even on a remote networked host.
Arco should be a nice alternative to `libpd` as an embedded sound
engine. The author is mainly using Arco in tandem with Serpent,
a real-time Python-like scripting language, but other language
bindings are possible.

If you are familiar with other computer music sound synthesis systems,
it may be useful to know how Arco is differs. Some of the distictive
features of Arco are:
 - **Embeddedness.** The main application envisioned is adding a flexible
   sound synthesis capability to an application or existing programming
   language. This calls for a small footprint rather than a large language
   implementation. Arco is also intended to run on microcomputers and
   single-board computers. Arco can run as a standalone server or it
   can be linked into an application or loaded as a language extension.
   DSP code is linked according to a manifest, so your application is not
   burdened with 100's of unit generator implementations you have no plans
   to use.
 - **O2.** In all configurations, Arco is based on O2, which provides
   message-based communications between the application or control
   language and the synthesis engine. O2 supports lock-free communication,
   real-time memory allocation, clock synchronization and other useful
   features. Built-in discovery lets you connect to an Arco server without
   messing with IP addresses or port numbers.
 - **Polymorphic Unit Generators.** Arco unit generators (dsp objects) are
   polymorphic to simplify their use, accepting different types of input
   that can change on the fly. For efficiency, there are 2 sample rates:
   audio rate, computed in blocks of 32 samples, and block rate at 1/32 of
   audio rate. A signal can also be a scalar value that changes when updated
   by a message. Any signal can have multiple channels. Code to handle mixed
   types of inputs is generated automatically and the implementation is very
   efficient. (A few things are prohibited: block-rate unit generators do not
   accept audio-rate inputs, and while any input can be single-channel, all
   multi-channel inputs must have the same number of channels.
 - **Language Independence.** Arco is agnostice about control, which can be
   located in an application or in another process, and control software can be
   written in any language. Pointers are avoided by "naming" unit generators
   with integers. Reference counting is used, although it can be largely hidden
   by libraries used on the control side, e.g. Serpent (programming language)
   garbage collects references to Arco, so users do not see reference counts.
 - **FAUST.** Unit generators can be written in FAUST, allowing FAUST programs
   to be used in a dynamic environment with real-time patching and control.
   Users can focus on using a library of ready-made unit generators, but
   Arco is not limited to a fixed DSP library since new algorithms can
   be written in FAUST and added with an automated build system.

## More Information
 - [Arco design](doc/design.md)
 - [Arco Unit Generator Reference](doc/ugens.md)
 - [Arco installation](doc/installation.md)
 - [Writing an Arco application](doc/building.md)
 - [What's where in the repository](doc/directories.md)
 - [Constraints - from GUI to DSP](doc/constraints.md)


