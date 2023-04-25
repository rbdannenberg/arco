# Arco Design Notes

**Roger B. Dannenberg**

## Contents:
- External Control
- Preferences
- Unit Generator Design
- The Ugen Abstract Class
- Memory Management
- Instruments in Serpent
- End-of-Sound Actions

## External Control
This section describes protocols and interaction between a client and
Arco, mainly the Audioio class that implements audio IO.

See also Internal States in `audioio.cpp`.

Initially, the audio thread is run by the host, which must call
`arco_thread_poll` periodically. If audio is being processed, the
audio thread control is safely handed off to audio callbacks, and
calls to `arco_thread_poll` will return immediately.

Arco receives `/arco/devinf`: gets return address and sends info
strings about audio devices, terminated by empty string.

Arco sends to reply address: info strings about audio devices.

Arco receives `/arco/reset`: shuts down audio thread, deletes all
Ugens, sets reply service (here, we assume it is "actl"), sends
confirmation. Do this first, even before `/arco/open`.
 
Arco sends `/actl/reset`: confirms audio stream closed and Ugens are
deleted. Client can send `/arco/open` again.

Client creates Zero and Thru Ugens for `INPUT_ID` and `PREV_OUTPUT_ID`
with:
- `/arco/zero/new ZERO_ID`
- `/arco/bzero/new ZEROB_ID`
- `/arco/thru/new INPUT_ID input_channels ZERO_ID`
- `/arco/thru/new PREV_OUTPUT_ID output_channels ZERO_ID`

Note that audio input and previous output are represented by Thru
Ugens whose outputs are written directly by the Audioio callback, and
their `real_run` methods are never called.
 
Arco receives `/arco/open`: opens audio IO, specifying devices,
channels, latency and buffer size (in frames). (Control or reply
service is also specified, but by convention it should be the same
service passed to `/arco/reset`. Here, we assume `/actl`.

Arco sends `/actl/starting`: gives actual device ids, actual channel
counts, actual latency and actual buffer size (in frames).

Arco receives `/arco/close`: close audio stream, but keep Ugens in
place.

Arco sends `/actl/closed`: confirms audio stream closed.

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

### The Ugen Abstract Class

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

## Unit Generator References in Serpent
Serpent could use the basic integer-as-reference interface as is,
but this means that Serpent users have to manage reference counts
if they want to dynamically create and destroy unit generators in
Arco. Serpent has garbage collection, so why not tie GC to Unit
Generators?

Unfortunately, Serpent does not have programmable actions when
Serpent objects are freed. This would require objects about to
be freed to execute an "about to be freed" method, which would
essentially create a new reference to the object and possibly
create additional references to the object, making it unsafe to
actually free it. Rather than solving this problem, we can make
"external" objects that run C++ code when they are freed. This
does not require entering the Serpent interpreter and as long as
we require that the C++ code does not create new references, the
object can be freed immediately after running the code.

For Ugens, we will create an external object called Ugen_id that
holds a 32-bit Arco id and a 32-bit epoch number. The epoch is
incremented whenever Arco is reset, clearing all unit generators.
After a reset, all references are invalid, so we need a way of
invalidating them.

How safe should this be? Since users can construct arbitrary O2
message and send to Arco, it's always possible to circumvent any
Ugen management mechanism, e.g. we can send a message like
`/arco/free 25`. Since this loophole is available, we can be
relaxed about other loopholes, e.g. we can allow Serpent programs
to extract integer id's from Ugen_id objects, but we want to
automate things for users as much as possible to avoid mistakes.

Currently, users normally send O2 messages from Serpent using
`o2_send_cmd` with a type string to specify types in the message.
We will add a special type character code "U" for Ugen that expects
a Ugen_id object, checks the epoch number to make sure it is valid,
extracts the Ugen_id, and adds it to the message using `o2_add_int32`.

Ugen_id objects are created by consulting an array of integers. The
array is initialized as a linked list, where each array element
contains the index of the next array element. To create a Ugen_id, pop
an entry off the linked list to get an integer. Mark the array element
at that index with its own index to mean *allocated*. Set the current
epoch number in the Ugen_id.

Note that Serpent can have any number of references to a Ugen_id and
it can be freely copied (because assignment of an object assigns the
object address). When a Ugen_id is freed by GC, we will send an
`/arco/free` message and return the id to the free list.

If we construct messages in the normal way, the message construction
primitives beginning with `o2_msg_start` and ending with
`o2_msg_finish` are *not* reentrant, which means we cannot send a
message from within the garbage collector, which can be invoked
between Serpent instructions. To solve this, we'll have the GC keep
another list of freed id's and provide a C++-level function that
once called runs without invoking GC and sends `/arco/free` messages
for all freed ids. We might optimize `/arco/free` by allowing it to
carry an arbitrary number of id's so we can free them in a batch.

Since Serpent will have to periodically call a function to check for
freed id's and send `/arco/free` there will be some delay between
freeing a Ugen on the Serpent (client) side and actually releasing the
id and Ugen on the Arco (server) side. There might be cases where we
want to release a Ugen immediately. We can add a `delete` method to
the Serpent Ugen class that makes the Ugen invalid. The method will
call a function that immediately sends `/arco/free` and marks the ugen
array entry with -id. Later, GC will actually free
the reference, and the -id will indicate that the `/arco/free` message
has been sent and the id can be moved to the free list (not the
to-be-freed list). (If the array entry is id, it means the id is
allocated. We put the array entry in the to-be-freed list which will
give the array entry the value to another index or -1 (end of
list). We want to delay moving the id to the free list for 
two reasons: (1) we need to keep the array entry -1 to tell GC that
the id is already free, and (2) if we immediately put the id back on
the free list, another object could be created for the id, leaving
twoand a second object would then have the same id number.

Functions available to Serpent are:
- `arco_ugen_new()` -- allocate an id and construct a new Ugen_id

- `arco_ugen_new_id(id)` -- construct a new Ugen_id from known id

- `arco_ugen_id(ugen_id)` -- extract the Arco id from the Ugen_id
  object. Returns -1 if the epoch is not current or ugen_id was freed
  using `arco_ugen_free`.

- `arco_ugen_epoch(ugen_id)` -- extract the epoch number from the Ugen_id
  object.

- `arco_ugen_free(ugen_id)` -- explicitly free the associated
  Ugen. The `arco_ugen_id` will become -1 for the remaining lifetime
  of this object, thus any references to the object will also lose
  their ability to reference the Ugen.

- `arco_ugen_gc()` -- when ugen_id objects are garbage collected,
  their id's are collected in a list, but the associated Ugens in Arco
  are not immediately freed. Calling `arco_ugen_gc` will free the
  associated Ugens in Arco and return their id's to the pool of unused
  id's on the Serpent side for reuse by `arco_ugen_new`. If there are
  no id's to be freed and more than half of the available Ugen id's
  are allocated, `arco_ugen_gc` runs an incremental step of
  garbage collection. Garbage collection is somewhat self-balancing in
  Serpent because more memory can be allocated. The more memory
  allocated, the more memory is freed after each mark-sweep cycle, so
  GC becomes more efficient when computation produces more
  garbage. But Ugens have a fixed set of id's and we do not want to
  run out. If we allocate lots of Ugens with relatively little
  computation (GC activity is roughly proportional to the amount of
  computation performed), we could run out of id's. Therefore, we want
  to spend more time on GC when we are running short of id's. Arco
  expects periodic calls to `arco_ugen_gc` even when there is nothing
  in particular to compute. This will give Arco a chance to make extra
  increments in the GC computation when running out of id's. If id's
  run out, it is pretty catastrophic, and the solutions are to
  recompile with a larger `UGEN_TABLE_SIZE` or to call `arco_ugen_gc`
  more often, or make more monolithic unit generators so you do not
  allocate so many in the first place.

- `arco_ugen_reset()` -- increment the epoch, invalidating all current
  Ugen_id objects. Returns the new epoch number.


## Instruments in Serpent
A client-side library for Arco is written in Serpent. You can use Arco
directly, but the approach with Serpent is to create classes and
objects that "shadow" the Arco objects and make it easier to allocate,
free, connect, and control a collection of Ugens in Arco.

In Serpent (using the arco.srp library), you create unit generators by
calling functions, e.g. `sine` to instantiate a `Sine` Ugen in the
Arco server. An important abstraction in this library is the
`Instrument` class, which represents a collection of unit generators
that are instantiated and destroyed as a unit. An `Instrument` has no
direct representation on the Arco server side. There, it is just a set
of independent unit generators. On the Serpent (client) side, the
`Instrument` is a real object. You can subclass `Instrument` to make
various kinds of instruments, inheriting the general behavior, which
includes a set of named components, named parameters that can be set,
a designated output unit generator that represents the output of the
`Instrument`, and a `free` operation that releases all the unit
generators in the `Instrument`.

To define an `Instrument`, the `init` method of a subclass calls
`instr_begin` which initializes a container for components of the
instrument. As each component is created, you call `member` to say
that the created Ugen is a member of the instrument, e.g.,
```
    var env = member(pwlb(dur * 0.2, 0.01, dur * 0.8), 'env')
```
This example creates a piece-wise linear envelope generator (`pwlb`),
adds it as a member to the instrument, and gives it the name
`'env'`. It also keeps a reference to the `Pwlb` in the local variable
`env`. Later, you can use `instrument.get('env')` to retrieve the
`pwlb` component, e.g. to start the envelope:
```
    instrument.get('env').start()
```
Alternatively, you could also store it
as an instance variable and write a `start` method for the instrument,
e.g.:
```
    def start(): env.start()  // new method in instrument
...
    instrument.start()  // called to start the instrument's envelope
```
You can also create named, setable parameters. The problem here is
that you might want your instrument to have parameters `freq` or
`amp`, but the instrument is not a real unit generator and has no
inherent parameters. You could write a method for each parameter, but
that is a little awkward. Consider implementing a settable frequency:
```
    var mysine  // this is in the class declaration

    def init():
        ...
        mysine = member(sine(220.0, 0.5), 'sine')
        ...

    def set_freq(f): mysine.set('freq', 440.0)
```
Then, you can change frequency by calling: `instrument.set_freq(440.0)`.

Alternatively, there is a special `set` method in
Instrument that avoids defining a method for every parameter.
To make this work, call `param` in `init`. For example, consider this
from the instrument `init` function:
```
    def init():
        ...
        var mysine = sine(220.0, 0.5)`, you can call:
        param(mysine, 'freq', 'myfreq')
        ...
```
This creates a Sine Ugen in the instrument and says that `'myfreq'`
will name the sine's `'freq'` parameter. Then, you can call
`instrument.set('myfreq', 440.0)`.


## End-of-Sound Actions
A general problem is: once you launch a collection of
Ugens to make a sound, how do you reclaim them when
the sound event completes?

A good example is the Pwlb subclass of Ugen, which is
a general-purpose piece-wise linear function generator
with output at the block rate, and typically used as
an amplitude envelope. We want to get a notice when
an instance of Pwlb reaches the last breakpoint
(typically with amplitude zero) in its list of breakpoints.

The mechanism is an O2 message to any designated service
with an ID number representing the end-of-envelope event.
To request a message, send `/arco/pwlb/act id action_id` to
the Pwlb instance (indicated by the `id` parameter). The
`action_id` is an arbitrary `int32` integer other than 0,
which means "no action notice." When the pwlb envelope ends,
a message is sent: `/actl/act action_id`, where `actl` is
the control service (`ctrlservice`) parameter passed in
the `/arco/open` message to initialize the Arco server.

### Serpent Implementaton
In Serpent, this mechanism is used to implement end-of-sound
actions. The most important is muting an instrument when an
envelope ends. To do this for an envelope `env`, we call
`env.atend(MUTE, this)`, where `atend` should be read
"at end," MUTE is the action to take, and `this` is an
optional parameter, which in this case refers to the
Instrument or Ugen we want to mute. (MUTE is the only implemented
action at this point.)

`atend` calls `create_action` which makes a unique new `action_id` and
sends `/arco/pwlb/act id action_id`. It also adds an action
description to a dictionary mapping action ids to action descriptions.

When the envelope ends, the handler for `/actl/act action_id`
retrieves the action from the dictionary and calls it, invoking
`instrument.mute()`, where `instrument` is the argument that was
the Instrument (`this`) passed to `atend`.  The `mute` method removes
the instrument from the Arco output collection and unreferences the
instrument, which normally frees the instrument and all its component
Ugens.

The `MUTE` action is only good for removing an Instrument or Ugen from
the output list in Arco, but the `create_action` method specify any
object, method and parameters to invoke, and you can even tie multiple
method invocations to a single end-of-envelope action.

The mixer `ins` method inserts a unit generator signal and a separate
gain into the mix, and there is an optional `atend` parameter which
can be one of:
- `SIGNAL` requires the unit generator to be an Instrument with a
  parameter named `envelope` that can send an action when it
  ends. When the envelope ends, the unit generator and the gain
  control are removed from the mix.

- `GAIN` requires that the gain be a `Pwllb` envelope. When the
  envelope ends, both the unit generator and gain control are removed
  from the mix.
  


