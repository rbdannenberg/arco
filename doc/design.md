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
- Phase Vocoder and Pitch Shifting
- Audio Shutdown

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
- `/arco/thru/new OUTPUT_ID output_channels ZERO_ID`

Note that audio input and previous output are represented by Thru
Ugens whose outputs are written directly by the Audioio callback, and
their `real_run` methods are never called.
 
Arco receives `/arco/open`: opens audio IO, specifying devices,
channels, latency and buffer size (in frames). (Control or reply
service is also specified, but by convention it should be the same
service passed to `/arco/reset`. Here, we assume `/actl`.

Arco sends `/actl/starting`: gives actual device ids, actual channel
counts, actual latency and actual buffer size (in frames).

If open is successful (indicated by actual_in_chans and
actual_out_chans both equal to zero), Arco later sends `/actl/started`
when audio callbacks are happening.

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

Initially, arco_* values are set to preferences. The can be changed.
Changes are saved in preferences.
Default in and out channels is 2. 

**Preferences inside/outside the server**
Applications pass in parameters to open audio: device ids, channel
counts, buffer size and latency. -1 works to get default values.

Preferences are on the "server side" and not visible to
the "arco side". To allow inspection of actual values selected, the
arco side sends actual values back to the control service, the name
of which is provided in /arco/ctrl.

On the "arco side", we have only defaults and whatever values are
passed into the open operation.

**Synchronization and Threads**

Audio runs off a callback, so we should be careful with threads.  All
communication is through O2, with preferences handled on the main
thread side. Actual values are sent from the audioio module via O2 so
either a shared-memory server or a remote application can work the
same way.

## Unit Generator Design

Unit generators are subclasses of Ugen (ugen.h). A Ugen has a rate
(either 'A' for audio or 'B' for block rate) and a Vec of outputs. To
make it easier to resolve types and perhaps to make code readable, the
output rate is determined by the choice of unit generator, e.g., use
`mult` for an audio-rate multiplier and `multb` for a block-rate
multiplier.

Inputs are more variable. The Ugen class does not specify any inputs,
but any subclass with inputs adds instance variables for: (pointer to)
input unit generator, stride for that unit generator (described
below), and pointer to input samples.

Ugen objects have a method `run` that is called on each input before
accessing input. The causes each input unit generator to compute its
output and may result in `run` calls to inputs of the inputs, etc.
The `run` method is inherited from `Ugen` and checks to see if output
is already up-to-date, which happens if the same output fans out to
multiple inputs -- only the first input calling `run` computes
anything, and other inputs reuse the output samples. If the output
samples are not up-to-date, `run` calls `real_run`, which each unit
generator overrides with its own DSP algorithm.

The trick is that there can be many different sorts of input
signals. With constant-, block-, and audio-rate inputs, and single-
vs. multi-channel signals, there are potentially 6 different
types. With N inputs, there are potentially 6^N variants of
`real_run`, which quickly gets out of hand, even with automatic code
generation.

To solve this problem, we use two main strategies. First, code to
iterate over multiple channels of input and output signals is
implemented in a single `real_run` method, but the processing of
different types of inputs is specialized by making an indirect method
call through `run_channel`, which points to a method.  E.g., in
`mult`, there are `run_channel` methods `chan_aa_a`, which multiplies
two audio-rate channels (each a vector of 32 floats) to produce an
audio-rate channel, and `chan_ab_a`, which mutiplies an audio-rate
channel by a block-rate channel (a single float). The variable
`run_channel` is set to the proper method whenever inputs change so
that the correct specialized DSP computation is run.

The second strategy reduces the logic to iterate over input channels
to a single add instruction for each input before calling
`run_channel`, so the overhead is the number of output channels x the
number of input signals. This is very small compared to the total
amount of data accessed and computed.

Each `run_channel` method expects input to start at the address in
per-input sample pointers. The pointers are not changed by
`run_channel`, but when it returns, sample pointers are incremented by
per-input stride quantities. For single-channel inputs, we want to
reuse the same input for each channel, so stride is 0. For
multi-channel audio inputs, we want to advance to the next channel of
the input. Input and output are consecutive in memory, so the stride
is the block length (BL = 32).  For multi-channel block-rate inputs,
there is only one sample per block, so the stride is 1. Finally,
constant inputs (values that can be updated by messages) are treated
the same as block-rate signals.  Constants can be single- or
multi-channel with strides of 0 or 1, respectively.

With these strategies, the combinatorics reduces from 6^N versions of
the inner computation loop to 2^N versions, because we need different
code for audio-rate and block-rate input. It is possible to restrict
this further by insisting that some inputs must be audio-rate, e.g. it
does not make much sense to apply an audio low-pass filter to a block
rate signal.

FAUST is used mainly to create the inner loops for `run_channel`
methods. A code generator, written in Python, extracts the inner loops
from FAUST code, does some rewriting, and generates code for Arco. To
generate all the variants of `run_channel`, FAUST is actually called
once for each variant, with source code generated from specification
file that includes both FAUST code and some annotations about which
variant are required. (link to details should go here.)

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

Computation basically involves a loop to compute each channel while
updating input and output pointers.

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

Arco allows unit generators to be allocated on-the-fly so that signal
processing can be reconfigured. An entire graph of unit generators can
be instantiated to make a sound and freed when the sound ends. Arco
uses reference counting to free unused unit generators.

Clients (applications) name unit generators with small
integers. Normally, the integer will index arrays on both the client
and Arco sides. The Arco side has an array of pointers to unit
generators, the `ugen_table`. Unit generators are referenced by this
table as well as by any unit generator that they are connected
to. Reference counts are decremented when a unit generator is
disconnected from an input. The reference from the `ugen_table` also
counts as one reference. You can free the integer index, which clears
the pointer from the `ugen_table` to the unit generator and decrements
the reference count. When the last reference is removed, the reference
count becomes zero and the unit generator is deleted.

In the simplest case, a tree of unit generators is created by the
client, which frees all but its reference ID to the output (Ugen at
the root of the tree). To produce audio output, the `/arco/output`
message inserts the output ugen into the `output_set`, which is a list
of ID's of unit generators whose outputs should be mixed to form the
audio output.

Eventually, the client can remove the ugen from the `output_set` using
`/arco/mute` and free the ID using the message address
`/arco/free`. If the object is not connected to any inputs, it will be
freed. The reference counts will propagate up the tree, freeing all
the unit generators in the tree.

You can also use `/arco/run` to insert a ugen ID into `run_set`, which
is a list of unit generators to "run" before computing each block of
audio output. Typically, unit generators in the `run_set` analyze
input audio (Arco's Vu meters are an example). Note that if a member
of the `output_set` depends directly or indirectly on another ugen,
that ugen will be run automatically (the tree of unit generators is
traversed in a depth-first manager, running all input-providers before
computing output). Therefore, anything that an output depends on does
*not* need to be added to the `run_set`.

### IDs as Cross-Thread References

On the client side, the integer "handles" or ID's for unit generators
can be managed in any way that is convenient. For small and mostly
static configurations, every unit generator ID can be stored in a
different variable for easy reference. The programmer must explicitly
free the ID by sending a message to `/arco/free` to delete the
object.

In Serpent, I have created shadow classes and objects for unit
generators, so the entire unit generator graph in Arco is mirrored in
the client side control program.  These objects encapsulate and manage
reference counts so that the client code is written in terms of
methods on the mirror objects. The client works as this level of
abstraction and there are no direct messages to Arco. Languages with
garbage collectors that call "cleanup code" when objects are freed
could use this to free Arco IDs.

In this more elaborate scheme, the ID on the client side is actually
an object, not a simple integer. When all references to the object
(and thus all ability to referene the acutal Arco ugen) are deleted
and the object is garbage colleted, we can `/arco/free` the ID, which
decrements the reference count and possibly deletes the ugen. (It could
still have other references within Arco and survive longer.)

Arco does not count `output_set` items as references, so reference
counts cna go to zero even while an object is being output. This
allows for implicit disconnection from output when an object no
longer has an id. Similarly, in Serpent, we do not keep a list of
playing objects, so object IDs are garbage collected and the
corresponding Arco object is deleted when there are no more
references to the shadow object in Serpent.


### Output and Run Sets

The `output_set` and `run_set` are lists of ugens and affect
reference counts. Every ugen also has flag bits `IN_OUTPUT_SET`
and `IN_RUN_SET` indicating membership. A ugen can only be in
either set once.


## Unit Generator References in Serpent

Serpent could use the basic integer-as-reference interface as is, but
this means that Serpent users have to manage reference counts if they
want to dynamically create and destroy unit generators in
Arco. Serpent has garbage collection, so why not tie GC to Unit
Generators?

Unfortunately, Serpent does not have programmable actions when Serpent
objects are freed. This would require objects about to be freed to
execute an "about to be freed" method, which would essentially create
a new reference to the object and possibly create additional
references to the object, making it unsafe to actually free it. Rather
than solving this problem, we can make "external" objects that run C++
code when they are freed. This does not require entering the Serpent
interpreter and as long as we require that the C++ code does not
create new references, the object can be freed immediately after
running the code.

For Ugens, we have an external object called Ugen_id that holds a
32-bit Arco id and a 32-bit epoch number. The epoch is incremented
whenever Arco is reset, clearing all unit generators.  After a reset,
all references are invalid, so incrementing the epoch number is a 
way of invalidating them.

How safe should this be? Since users can construct arbitrary O2
message and send to Arco, it's always possible to circumvent any Ugen
management mechanism, e.g. we can send a message like `/arco/free
25`. Since this loophole is available, we can be relaxed about other
loopholes, e.g. we can allow Serpent programs to extract integer id's
from Ugen_id objects, but we want to automate things for users as much
as possible to avoid mistakes.

Currently, users normally send O2 messages from Serpent using
`o2_send_cmd` with a type string to specify types in the message.  We
will add a special type character code "U" for Ugen that expects a
Ugen_id object, checks the epoch number to make sure it is valid,
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
another list of freed id's and provide a C++-level function that once
called runs without invoking GC and sends `/arco/free` messages for
all freed ids. We might optimize `/arco/free` by allowing it to carry
an arbitrary number of id's so we can free them in a batch.

Since Serpent will have to periodically call a function to check for
freed id's and send `/arco/free` there will be some delay between
freeing a Ugen on the Serpent (client) side and actually releasing the
id and Ugen on the Arco (server) side. There might be cases where we
want to release a Ugen immediately. We can add a `delete` method to
the Serpent Ugen class that makes the Ugen invalid. The method will
call a function that immediately sends `/arco/free` and marks the ugen
array entry with -id. Later, GC will actually free the reference, and
the -id will indicate that the `/arco/free` message has been sent and
the id can be moved to the free list (not the to-be-freed list). (If
the array entry is id, it means the id is allocated. We put the array
entry in the to-be-freed list which will give the array entry the
value to another index or -1 (end of list). We want to delay moving
the id to the free list for two reasons: (1) we need to keep the array
entry -1 to tell GC that the id is already free, and (2) if we
immediately put the id back on the free list, another object could be
created for the id, so a second object would then have the
same id number.

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
`instr_begin()` which initializes a container for
components of the instrument. Let's create an instrument `Twoharm`
that simply adds two sinusoids to create two harmonics:
<!--
### *member() is obsolete*
*As each component is created, you call*
*`member` to say that the created Ugen is a member of the instrument,*
*e.g.,*
```
    var env = member(pwlb(dur * 0.2, 0.01, dur * 0.8), 'env')
```
*This example creates a piece-wise linear envelope generator (`pwlb`),*
*adds it as a member to the instrument, and gives it the name*
*`'env'`. It also keeps a reference to the `Pwlb` in the local variable*
*`env`. Later, you can use `instrument.get('env')` to retrieve the*
*`pwlb` component, e.g. to start the envelope:*
```
    instrument.get('env').start()
```
-->
```
class Twoharm (Instrument): 
    def init(): 
        instr_begin() 
        var twoharm = add(sine(220.0, 0.1), sine(440.0, 0.1)) 
        super.init("Twoharm", twoharm) 
```
Here, `"Twoharm"` is the instrument name (used for text descriptions
in console output) and `twoharm` is passed to `super.init` to indicate
the output. For example, you can now write `Twoharm().play()` to play
the instrument and the Ugen represented in `init` by `twoharm` will be
the one connected to the output mixer.

## Parameters

### Simple Methods 

You can implement methods to update parameters. For example, here is
a simple way to change the frequency of a `Twotone`:
```
class Twoharm (Instrument): 
    var h1, h2

    def init(freq): 
        instr_begin() 
        h1 = sine(freq, 0.1)
        h2 = sine(freq * 2, 0.1)) 
        var twoharm = add(h1, h2)
        super.init("Twoharm", twoharm) 

    def set_freq(x):
        h1.set('freq', x)
        h2.set('freq', x * 2)
```
Now you can write `var twoharm = Twoharm(220.0)` to play a 220 Hz tone
and then `twoharm.set_freq(330.0)` to change the frequency to 330 Hz.

You could almost pass in a signal to the `Twoharm` constructor to get
continuous frequency control, but to make this work, you would need to
change `freq * 2` to `mult(freq, 2)` since the `*` operator only works
on numbers.

### Implementing `set()` Style Updates

The "standard" way to update Ugens is to call `ugen.set('attr',
value), where `'attr'` is some attribute name like `'freq'` or
`'amp'`. This interface is recommended for Instruments as well, and is
supported by some special functions. To change our `Twoharm` example
to accept `.set('freq', x)`, we rewrite it as follows:
```
class Twoharm (Instrument): 

    def init(freq): 
        instr_begin()
        freq = param('freq', freq)
        var twoharm = add(sine(freq, 0.1), sine(multb(freq, 2), 0.1))
        super.init("Twoharm", twoharm) 
```
A bit of "magic" here is that `param` returns a `Param_descr` that you
can (mostly) treat as a signal that you can pass as a parameter to any
Ugen. Note how we can write `sine(freq, 0.1)` and `multb(freq,
2)`. However, now when you write `twoharm.set('freq', x)` the value of
`x` (either a signal or a float or an array of floats) will be
propagated to every Ugen the previously received the `Param_descr`.

For example, if you write `twoharm.set('freq', 330.0)`, it will update
the frequency of an instance of `Twoharm`, changing both harmonics.

Note that we no longer need explicit instance variables `h1` and
`h2` to remember the `Sine` ugens, and `def set_freq ...` is no longer
needed. The `param` function works with `instr_begin()`,
`Instrument.init()`, and `Ugen.init()` (called by `Sine.init()`) to
create a mapping from the attribute `'freq'` to the inputs of the
`Sine` and `Mathb` (implementing `multb`) Ugens.

### The param Function and Options
The `param` function has some optional parameters that allow the
incoming parameter to the Instrument to be linearly mapped from a
range of [0, 1] to any range [low, high]. You can also clip the input
values to the range [low, high] (with or without initial mapping).

You can also apply smoothing to float updates with a setable time
constant. For example, if you connect a slider control to an
amplitude, smoothing the signal will avoid "zipper" noise due to small
but sudden changes in amplitude. See `param`'s implementation in
`instr.srp`.

Because of the mapping and clipping feature, it is convenient to
"convert" an incoming parameter like `freq` into a `Param_descr` as
shown in the previous example. If `myparm` is a parameter to an
Instrument subclass, then `myparm = param('myparm', myparm, 'clip', 0,
1)` converts `myparm` to a `Param_descr`. However, if `myparm` was
passed in as a float, you could write `myparm * 2` or any other float
operation. Now you cannot do that. If `param` does any mapping But
instead, you can write `myparm.value() * 2`. This is probably better
than saving and using the original float parameter because
`myparm.value()` has clipping applied. Note that if the original
parameter was an array of floats (representing a multi-channel
"float") or a non-Const-like Ugen, then `myparm.value()` will
return either the array of floats or the Ugen.

A "Const-like" Ugen is either a Const or Smoothb. Both of these are
Ugens with block-rate outputs that essentially output constant values
as opposed to time-varying signals. The can be set with a `.set(x)`
method, and they can have multiple channels. The roll of Const-like
Ugens is to allow floats to be used as Ugen inputs without
implementing a special case within every Ugen. Instead, the floats are
coerced to either a Const or Smoothb. A float is coerced to a
Const-like when `Param_descr` is provided as a Ugen signal input.
Smoothb Ugens differ from Const mainly by applying smoothing so their
outputs do not change instantly.

### Implementing Updates with Methods

Sometimes, the functional style of `param` is not enough and you need
to invoke a method to implement an operation like `.set('freq',
x)`. Modifying our example again, here is how to do it:

```
class Twoharm (Instrument): 
    var h1, m2

    def init(freq): 
        instr_begin()
        freq_param = param('freq', freq)
        param.add_method(this, 'set_freq')
        h1 = sine(freq, 0.1)
        m2 = multb(freq, 2)
        var twoharm = add(h1, sine(m2, 0.1))
        super.init("Twoharm", twoharm) 

    def set_freq(x):
        h1.set('freq', x)
        m2.set('x1', x)
```
Here, we revert back to saving some Ugens as instance variables so we
 can reference them in the `set_freq` method. This version uses `m2`
 for the 2nd harmonic's multiplier Ugen (`multb`) rather than the
 `Sine` so that  `set_freq` will work with signals as well as
 floats. (This comes at the small cost of running the multiply for
 *every* block of 32 samples rather than doing it once at update
 time. You could optimize this with some careful coding in `set_freq`
 but the savings would be minimal.)

See also [Instrument Parameters](./instruments.md) and the `param()`
function described there.

## End-of-Sound Actions

A general problem is: once you launch a collection of Ugens to make a
sound, how do you reclaim them when the sound event completes?

A good example is the Pwlb subclass of Ugen, which is a
general-purpose piece-wise linear function generator with output at
the block rate, and typically used as an amplitude envelope. We want
to get a notice when an instance of Pwlb reaches the last breakpoint
(typically with amplitude zero) in its list of breakpoints.

The mechanism is an O2 message to any designated service with an ID
number representing the end-of-envelope event.  To request a message,
send `/arco/pwlb/act id action_id` to the Pwlb instance (indicated by
the `id` parameter). The `action_id` is an arbitrary `int32` integer
other than 0, which means "no action notice." When the pwlb envelope
ends, a message is sent: `/actl/act action_id`, where `actl` is the
control service (`ctrlservice`) parameter passed in the `/arco/open`
message to initialize the Arco server.

### Sounds that End

Setting up end-of-sound actions is not trivial. Consider playing a
sound from a file into a mixer input. When the sound (from a Recplay
object) finishes, we want to remove the sound from the mixer, but
that means (1) remember a name for the mixer input, (2) schedule an
end-of-sound action that takes the name and mixer as parameters,
(3) write the end-of-sound method to uninsert the named ugen from
the mixer. Simply invoking a built-in action like MUTE or FINISH
is not sufficient.

Alternatively, in Nyquist, the file playback sound would simply
end. The "mixer" would consist of a gain multiplier and an adder.
The gain multiplier would detect a terminated input, and because
multiplication by zero is zero, the product sound would terminate.
Then, addition by zero is the identity, to the adder would
be replaced by the other adder input (add are always binary, so
a mixer would have a chain or tree of adders).

We can have something similar in Arco. Termination could be a bit
maintained by every Ugen. The run method, before calling real_run(),
can check for termination. If terminated, it can just return out_samps
without further evaluation. The caller can also check the terminate
bit and invoke some action; e.g. Mix and Add should remove inputs that
terminate, and filters should terminate *their* output when their
input terminates, but possibly after a delay or detecting the output
had decayed to near zero.

However, we need to make termination an option; e.g. even envelopes
in Arco can be re-started, so automatically terminating everything
that *seems* to stop is not workable. That also means that when a
unit generator is created, the default should be that it runs forever
and an option is for it to report termination.

What would a filter look like in this scheme? Instead of
```
    void real_run() {
        snd_samps = snd->run(current_block); // update input
        cutoff_samps = cutoff->run(current_block); // update input
        ...
```
we could write
```
    void real_run() {
        snd_samps = snd->run(current_block); // update input
        cutoff_samps = cutoff->run(current_block); // update input
        if ((snd_samps->flags & cutoff_samps->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            flags |= TERMINATED;  // propagate terminated input to output
        }
        ...
```
Note that termination will propagate without delay. What if we need to
delay propagating termination until the output decays to zero? We do
not have timed events within the audio thread, so we have to use a
counter. Overhead is only incurred after an input terminates:
```
    void real_run() {
        snd_samps = snd->run(current_block); // update input
        cutoff_samps = cutoff->run(current_block); // update input
        if ((snd_samps->flags & cutoff_samps->flags & TERMINATED) &&
            (flags & CAN_TERMINATE)) {
            terminate();
        }
        ...
    }
    
    // this is inherited from Ugen. tail_blocks is a new member
    // variable of Ugens that is initialized to zero, but set by
    // the method that sets CAN_TERMINATE.
    void terminate() { // this Ugen will terminate after tail_blocks
        if (!(flags & TERMINATING)) {
            terminate_count = tail_blocks;
            flags |= TERMINATING;  // start the count
        }
        if (terminate_count-- == 0) {
            flags |= TERMINATED;
            on_terminate();  // a virtual method called at most once
        }
    }
    
```



### Serpent Implementaton

In Serpent, this mechanism is used to implement end-of-sound
actions. The most important is muting an instrument when an envelope
ends. To do this for an envelope `env`, we call `env.atend(MUTE,
this)`, where `atend` should be read "at end," MUTE is the action to
take, and `this` is an optional parameter, which in this case refers
to the Instrument or Ugen we want to mute. (MUTE is the only
implemented action at this point.)

`atend` calls `create_action` which makes a unique new `action_id` and
sends `/arco/pwlb/act id action_id`. It also adds an action
description to a dictionary mapping action ids to action descriptions.

When the envelope ends, the handler for `/actl/act action_id`
retrieves the action from the dictionary and calls it, invoking
`instrument.mute()`, where `instrument` is the argument that was the
Instrument (`this`) passed to `atend`.  The `mute` method removes the
instrument from the Arco output collection and unreferences the
instrument, which normally frees the instrument and all its component
Ugens.

The `MUTE` action is only good for removing an Instrument or Ugen from
the output list in Arco, but the `create_action` method can specify
any object, method and parameters to invoke, and you can even tie
multiple method invocations to a single end-of-envelope action.

The `FINISH` action is a more general action that invokes the `finish`
method of the instrument, where "the instrument" means the `this`
parameter passed to `atend`, e.g. `x` in `env.atend(FINISH, x)`. In
this case, `finish` is passed a *status* parameter, so it should
probably be declared `def finish(optional status)`.

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

## Phase Vocoder and Pitch Shifting

These are notes on the `pv` unit generator. For pitch shifting, we
need to stretch by ratio and then downsample by ratio. Should we
stretch first or resample first? One reason to stretch first is that
the pitch can be changed instantly by changing resampling, as opposed
to resampling first and then adding the latency and uncertainty of the
FFTs used for stretching.

In the CMUPV library, `ratio` is the reciprocal of stretch, so `ratio > 1`
means the output sound will be shorter than the input sound.

For the Pv Unit Generator, `ratio` is the stretch factor *and* the
pitch shift ratio, so `ratio > 1` means we stretch the input to get a
longer intermediate CMUPV output. Then we resample this output to be
shorter, causing the pitch to be higher.

In this description we use the Pv UG definition of `ratio`, and may
refer to the CMUPV library ratio as `pvratio`.

Let the stretch ratio and pitch shift ratio be R, so resampling ratio is 1/R.
To stretch, we need N samples in a buffer, where N is the FFT size. The
FFT steps through the buffer by N/8 when the ratio is 1, so it steps by
N/(8R) to stretch by R. We want the FFT to operate on contiguous samples.
We could shift the buffer by N/(8R) after each FFT, or we could make the
buffer longer and shift only when we need to. That seems to be a better
plan and easier to implement, as it handles cases where R changes often.
So let's make the buffer 2N in size, restricting R to >1/8. (Although for
shifts of more than 3 octaves, ratio = 8, the reconstruction hop size can
be arbitrarily small, leading to greater overlap of the analysis windows.)

To minimize latency, we will let the resampling drive the phase vocoder.
There is an input buffer of size 2N. When the input would overflow the
buffer, the buffer is shifted by N samples, which retains N samples for
an FFT, and opens space for the next N samples. Therefore, an FFT can
always be computed from a continguous span of the most recent N samples.

The phase vocoder algorithm produces H samples after each FFT, where
H is the hop size. An output buffer contains the signal to be resampled.
This buffer is of size H + M, where M is the number of samples of history
needed for resampling. E.g. for 9 point sinc interpolation, M = 4.
Output is generated by resampling the output buffer. When the resampling
reaches the end of the output buffer, the output buffer is shifted by H
and H samples are obtained from the phase vocoder to refill the buffer.

When new samples are needed for the output directory, we need to determine
where to compute the next FFT. One natural approach would be to advance
the analysis window by H/R to compensate for the resampling. However, if
the resample rate is changed continuously and if the analysis window location
is quantied to the nearest sample, there is the risk that the time stretching
and resampling will not exactly match, causing either buffer overflow or
buffer underflow. A simpler scheme is simply to compute an FFT of the *last*
available N samples in the input buffer. This will automatically minimize
the latency and avoid any rounding or quantization errors.

Does it matter that input arrives in blocks of 32 or that FFTs will always
be aligned to 32-sample boundaries? I think since the FFTs are in the 1024
to 4096 size range that shifting by 32 samples will have no audible effect
as long as the phase is adjusted accordingly.

This approach, where we take the most recent N samples as the analysis
window, is not standard for phase vocoders, so we'll need to interface
with the phase vocoder to cause it to compute an input hop size that moves
the FFT window to the end of the buffer. With CMUPV, the API says we can
set ratio before each request for an output block. We will use an output
block size of H so that each output block will require exactly one analysis
window, so we can adjust ratio before each analysis. With CMUPV, we want
to set ratio such that `pv_get_input_count` returns the right number of
samples. E.g. suppose the last window ended at I and the last sample in
the buffer is at J. Then we want `pv_get_input_count` to return J - I.

Reading through `pv_get_input_count` code, we see
`ana_hopsize = lroundf((pv->syn_hopsize) * (pv->ratio));`
so roughly, `ana_hopsize = H * ratio` and
`int need = pv->blocksize - (int)(pv->frame_next - pv->out_next);`
Since we are requesting block sizes equal to the hop size, `need = H`.
Next, `int frames = (need + pv->syn_hopsize - 1) / pv->syn_hopsize;`
so `frames = 1` as expected.
Next, `pv_get_input_count` computes
`pv->input_head += ana_hopsize;` and then
`need = pv->fftsize + ana_hopsize * (frames - 1);` but `frames == 1`,
so `need == fftsize` (yes, we're expecting to need a single FFT frame).
Next, `long have = (long) (pv->input_rear - pv->input_head);` where
`input_rear` is the index of the end of the input data, which should be
the end of the previous FFT analysis window. Next, we have `need -= have`.
Let `prevwin` be the location of the previous window. Assuming we had
just enough samples for that window, `pv->input_rear` is the end of the
bufffer = `prevwin + fftsize`, `pv->input_head = prevwin + ana_hopsize`,
`have = pv->input_rear - pv->input_head`
`= (prevwin + fftsize) - (prevwin + ana_hopsize)`
`= fftsize - ana_hopsize`, so after `need -= have` we get
`need = fftsize - (fftsize - ana_hopsize) = ana_hopsize`. So CMUPV
is going to ask for `ana_hopsize` (based on ratio) every time we ask
for more output. So this is actually very simple: Initially, we zero
the input buffer, consider the buffer to contain N samples, expect the
first request for samples to ask for a full frame of N samples, which
we deliver from sample 0 to N-1.

We remember `prev_input_end` as the location of the end of the
previous input to the phase vocoder (passed in by `pv_put_input()`).
We want the next input to consume all the samples from
`prev_input_end` to the end of the input buffer, which we can call
`available`. Therefore, we set ratio such that 
`available == lroundf(H * ratio)`, or `ratio = available / H`, or
given that CMUPV uses the reciprocal of this, `pvratio = H /
available`.

There is a limit to `ana_hopsize`, so `ratio` cannot be too small.
Setting  `ratio` too low can cause the input buffer to overflow.
The maximum hopsize is N / 3 and the "normal" hopsize for ratio = 1
is N / 4, so the smallest ratio is 3 / 4. However 4 is just N / H, so
in general, the smallest ratio is 3 * H / N. E.g. if we set H to N / 8,
the smallest ratio is 3 / 8, or more than 1 octave down, and H = N / 16,
e.g., N = 2048, H = 128, allows more than 2 octaves down. The unit
generator should limit ratio to be > 3 * H / N to avoid unworkable
parameters.

### Sinc interpolation
Following Analog Devices *dsp_book_Ch16.pdf*, we will use a windowed
sinc function and a Blackman window:
```
w[i] = 0.42 - 0.5 * cos(2*PI*i/M) + 0.08 * cos(4 * PI * i/M)
```
where M is the number of points - 1.
From this, we'll construct a windowed sinc function. When we up-sample,
the sinc function is a reconstruction filter to interpolate between
the input samples, so the zeros are spaced according to the input. The
sinc function has a span of 9 input samples.

When we down-sample, the sinc function is a low-pass filter with zeros
spaced according to the output. This means we will sum over about 8/R
samples, e.g. if we downsample by one octave, we have to sum over 16
points. We could use a constant number, but that would require a different
kernel for each ratio R, and that would be very expensive to
construct.

There is some code to run tests on resampling in arco/cmupv/src. Some
results and comments in arco/cmupv/src/testdata.txt say:

Notes: Large windows work best, so results with a small window of, say,
8 may not be great. With a window size of 8, transposing down seems to
work better, and transposing down really sums 8 points (4 on each side),
whereas when you tranpose up, you need to lowpass to reduce aliasing,
and for that the window size is greater than 8; e.g., if you transpose
up an octave, the windowed sinc function has to be twice as wide, so it
spans 16 samples of input.

For 8 (which we should at least try because it is much faster than
a really accurate interpolation), an oversampling factor of 32 seems
reasonable. The windowed sinc function takes 8 * 32 * 4 * 2 = 2048 bytes,
so not a problem. Unfortunately, substantial gains in performance
require larger windows, not better interpolation.

It seems like 8-sample windows do not do much for anti-aliasing, i.e.,
just not a sharp cutoff at the Nyquist frequency. Getting a faster
cutoff requires a long filter, so maybe it is not suitable for real-time,
and the best approach is simply to transpose down, not up.

Execution time with a window of 8 is about 2000 times real time.

## Audio Shutdown

How does Arco shut down in a coordinated fashion?

### Running the Audio thread

See comments at the top of audioio.cpp with an analysis/description of
aud_state transitions.
