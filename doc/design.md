# Arco Design Notes

**Roger B. Dannenberg**

## Preferences
Preferences within Arco source code are confusing enough that you
will need this to understand the code:

There are several sets of preference variables with different prefixes:
- `p_*` -- private to prefs module; values correspond to .arco prefs file
- `arco_*` -- Used for getting preferences from the user interactively.
- `actual_*` -- Parameters determined after devices were actually opened.
  These values are displayed in interfaces as the "Current value" of any
  parameter.

These prefixes are applied to each of the following suffixes. Here is
what they mean:

- `*_in_id` -- PortAudio input device number
- `*_out_id` -- PortAudio output device number
- `*_in_name` -- PortAudio input device name (or substring thereof)
- `*_out_name` -- PortAudio output device name (or substring thereof)
- `*_in_chans` -- input channel count
- `*_out_chans` -- output channel count
- `*_buffer_size` -- samples per audio buffer (not necessarily the
  same as the Arco block length, BL, as Arco may need to compute
  multiple blocks to fill the buffer on each audio callback).
- `*_latency` -- output latency in ms (the output buffer should be
  about this long, or if double-buffered, each buffer should be about
  this long. The input buffer should also be this long, but we assume
  it is empty when the output buffer is full, so input buffers do not
  contribute additional latency.)

**General information flow:**
- `actual_*` values are provided by PortAudio when devices are opened.
They are used by the audio callback to interpret the audio stream.
- `p_*` values are initialized from the preferences file `.arco`
- `arco_*` values are initialized from `p_*` values, and when the user
  sets valid `arco_*` values, they are used to update `p_*` values.
  (Setting to -1 or some indication of "default" is also valid and
  means "use default value instead of whatever might have been in the
  preferences file", i.e., "clear the preference".)
- Values used to open PortAudio devices are the `p_*` values unless
  they are -1 in which case default values are used.

Preferences are only written on request by writing the current `p_*`
values, which can include -1 for "no preference".

**Preferences inside/outside the server**
Applications pass in parameters to open audio: device ids, channel
counts, buffer size and latency. -1 works to get default values,
but it is up to applications to manage preferences.

Therefore preferences are on the "server side" and not visible to
the "arco side". To allow inspection of actual values selected, the
arco side sends actual values back to the control service, the name
of which is provided in /arco/open.

On the "arco side", we have only defaults and whatever values are
passed into the open operation.

**Synchronization and Threads**

Audio runs off a callback, so we should be careful with threads.
All communication is through O2, with preferences handled on the
main thread side. Actual values are sent from the audioio module
via O2 so either a shared-memory server or a remote application
can work the same way.

## Unit Generator Design
Unit generators are subclasses of Ugen (ugen.h). A Ugen has a
rate (either 'A' for audio or 'B' for block rate) and a Vec of
outputs. To make it easier to resolve types and perhaps to
make code readable, the output rate is determined by the choice
of unit generator, e.g., use `mult` for an audio-rate multiplier
and `multb` for a block-rate multiplier.

Inputs are more variable. The Ugen class does not specify any
inputs, but any subclass with inputs adds instance variables
for: (pointer to) input unit generator, stride for that unit
generator (described below), and pointer to input samples.

Ugen objects have a method `run` that is called on each input
before accessing input. The causes each input unit generator to
compute its output and may result in `run` calls to inputs of
the inputs, etc.  The `run` method is inherited from `Ugen`
and checks to see if output is already up-to-date, which happens
if the same output fans out to multiple inputs -- only the first
input calling `run` computes anything, and other inputs reuse
the output samples. If the output samples are not up-to-date,
`run` calls `real_run`, which each unit generator overrides
with its own DSP algorithm.

The trick is that there can be many different sorts of input
signals. With constant-, block-, and audio-rate inputs, and
single- vs. multi-channel signals, there are potentially 6
different types. With N inputs, there are potentially 6^N
variants of `real_run`, which quickly gets out of hand, even
with automatic code generation.

To solve this problem, we use two main strategies. First,
code to iterate over multiple channels of input and output
signals is implemented in a single `real_run` method, but
the processing of different types of inputs is specialized
by making an indirect method call through `run_channel`,
which points to a method.  E.g., in `mult`, there are
`run_channel` methods `chan_aa_a`, which multiplies two
audio-rate channels (each a vector of 32 floats) to
produce an audio-rate channel, and `chan_ab_a`, which
mutiplies an audio-rate channel by a block-rate channel
(a single float). The variable `run_channel`
is set to the proper method whenever inputs change so that
the correct specialized DSP computation is run.

The second strategy reduces the logic to iterate over
input channels to a single add instruction for
each input before calling `run_channel`, so the overhead
is the number of output channels x the number of input
signals. This is very small compared to the total
amount of data accessed and computed.

Each `run_channel` method expects input to start
at the address in per-input sample pointers. The
pointers are not changed by `run_channel`, but
when it returns, sample pointers are incremented by
per-input stride quantities. For single-channel
inputs, we want to reuse the same input for each
channel, so stride is 0. For multi-channel audio
inputs, we want to advance to the next channel of
the input. Input and output are consecutive in
memory, so the stride is the block length (BL = 32).
For multi-channel block-rate inputs, there is only
one sample per block, so the stride is 1. Finally,
constant inputs (values that can be updated by
messages) are treated the same as block-rate signals.
Constants can be single- or multi-channel with strides
of 0 or 1, respectively.

With these strategies, the combinatorics reduces from
6^N versions of the inner computation loop to 2^N versions,
because we need different code for audio-rate and block-rate
input. It is possible to restrict this further by insisting
that some inputs must be audio-rate, e.g. it does not make
much sense to apply an audio low-pass filter to a block rate
signal.

FAUST is used mainly to create the inner loops for
`run_channel` methods. A code generator, written in Python,
extracts the inner loops from FAUST code, does some rewriting,
and generates code for Arco. To generate all the variants of
`run_channel`, FAUST is actually called once for each
variant, with source code generated from specification file
that includes both FAUST code and some annotations about
which variant are required. (link to details should go here.)

### The Ugen abstract class

Every Ugen has:
- `rate`: `'a'` (audio), `'b'` (block), or `'c'` (rate)
- `chans`: number of output channels
- `block`: block count, intially -1. Updated to current block number by `run(block)`
- `output`: a vector of Samples. 
  - A-rate output has one audio block per channel, flattened to an array of length `chans * BLOCK_LEN`. 
  - B-rate output has `chans` samples.
- `states`: a vector of `chans` structures, where each structure contains all
   the state for one channel of the unit generator
- `inputs`: for each input, there is a source pointer of type `Ugen *` and a pointer to the Ugen's output vector and a stride.

Computation basically involves a loop to compute each channel while updating input
and output pointers.

Inputs must have either 1 channel or the same number of channels as the Ugen. If 1 channel, the same channel is used for each channel of the Ugen.
- audio rate input
  - 1 channel: *param*_stride = 0
  - *n* channel: *param*_stride = BL // next channel is where previous one ends
- block rate input
  - 1 channel: *param*_stride = 0
  - *n* channel: *param*_stride = 1 // 1 sample per channel
- constant rate input
  - 1 channel: *param*_stride = 0
  - *n* channel: *param*_stride = 1 // 1 sample per channel


## Memory Management
Arco allows unit generators to be allocated on-the-fly so
that signal processing can be reconfigured. An entire graph
of unit generators can be instantiated to make a sound and
freed when the sound ends. Arco uses reference counting to
free unused unit generators.

Clients (applications) name unit generators with small
integers. Normally, the integer will index arrays on both
the client and Arco sides. The Arco side has an array of
pointers to unit generators, the `ugen_table`. Unit
generators are referenced by this table as well as by
any unit generator that they are connected to. Reference
counts are decremented when a unit generator is
disconnected from an input. You can also free the integer
index, which clears the pointer from the `ugen_table` to
the unit generator. When the last reference is removed,
the reference count becomes zero and the unit generator
is deleted.

In the simplest case, a tree of unit generators is
created by the client, which frees all but its reference
ID to the output (Ugen at the root of the tree). The
output is connected to Arco's audio output object, so
the reference count there is 2.
Later, the client can disconnect the output and it
will become dormant, but could be reconnected in the
future. Ultimately, the client can free the ID
using the message address `/arco/free`. If the object
is not connected to any inputs, it will be freed. The
reference counts will propagate up the tree, freeing
all the unit generators in the tree.

On the client side, the integer "handles" or ID's for
unit generators can be managed in any way that is
convenient. For small and mostly static configurations,
every unit generator ID can be stored in a different
variable for easy reference. The programmer must explicitly
free the ID by sending a message to `/arco/free` to delete
the object. In Serpent, I have created shadow classes and
objects for unit generators, so the entire unit generator
graph in Arco is mirrored in the client side control program.
These objects encapsulate and manage reference counts so
that the client code is written in terms of methods on the
mirror objects. The client works as this level of abstraction
and there are no direct messages to Arco. Languages with
garbage collectors that call "cleanup code" when objects
are freed could use this to free Arco IDs.


