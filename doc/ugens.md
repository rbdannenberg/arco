# Arco Unit Generator Reference

**Roger B. Dannenberg**

## Introduction
Unit generators are "named" by small integers. It is up to the client to
select the integer. (Normally, you would expect an API to provide an
"allocate" function that returns the name of a new object, but since
Arco is an asynchronous process, we want to avoid synchronous calls or
functions that return values wherever possible.)

### O2 Addresses
O2 messages to Arco address classes and methods rather than objects.
You might expect something like `/arco/25/set_freq` to set the frequency
of object with ID 25, but this design would require us to install
multiple new addresses in the O2 address space each time we create
an object. Instead, we write `/arco/sine/set_freq <id> <chan> <freq>`.
Note that the object ID is now a parameter. This can be resolved to a
Ugen by simple table lookup.

### Naming Conventions
O2 addresses generally have the form `/arco/<class>/<method>` and methods
often include the forms:
 - `new` creates a new instance of the class. Parameters are the ID, the
   number of output channels, and an integer ID for each input.
 - `set_<input>` for setting an input to a constant value (the input
   value is a float). The input must be a constant.
 - `repl_<input>` for setting an input to the output of another object
   (the input "value" is an integer ID for another object).

### Creating and Updating a Ugen
To create a Ugen:
````
/arco/fmosc/new ID chans input1 input2 ...
````
where inputs are ID's for other UGens.

To release a Ugen, free the ID, but the ugen is actually deleted only when all references to it are deleted. When a Ugen is deleted, its references are also deleted, which may cause other Ugens to be deleted.
````
/arco/free ID
````

To change a parameter, there are two possibilities. A constant (a Ugen of class Const)
can be updated with a new value. A constant can have multiple channels (so it can be
seen as a vector of floats), so the update includes a channel. You can update a
constant directly using:
```
/arco/const/set ID chan float
```
But you can also free the Const Ugen after it has been assigned as the input to
some other Ugen. Since the refcount is greater than 1, releasing the ID does not
delete the constant, and some Ugen holds the reference. But every Ugen has
commands to follow the reference to the Const object and set a value:
````
/arco/fmosc/set_freq ID chan float
````
This message only works if the current input is a Const and is recommended because the client can free its direct reference to the Const object. Also, it seems more direct to set an *input* parameter of the ugen as opposed to an *output* parameter of a Const.

As a further convenience and matter of style, the client can allocate fixed
Arco ID numbers for Const construction. When you want a "c-rate" (i.e. Const) input,
send `/arco/const/new` using the fixed ID, use `/arco/const/set` to set the value(s),
and then use this Const as a parameter for a new Ugen. Immediately send `/arco/free`
to free the Const. Since the const is still referenced by the new Ugen, you can
use messages like `/arco/fmosc/set_freq` to change the Const value, so you really
do not need to retain a direct reference to the Const, and now you can reuse the
fixed ID number for the next Const parameter you need. Since Ugens can have multiple
parameters, you should reserve a handful of Const IDs. We use 5, 6, 7, 8, and 9.

In summary, the pattern for starting Ugens with one or more Const inputs is:
1. Create Const objects for each Const parameter using fixed Arco ID numbers.
2. Set the initial values of the Const objects.
3. Pass the objects as parameters to the newly created Ugen.
4. Free all the Const objects (from their IDs).
5. If you need to change an input value, do it through one of the Ugen's 
      `set_` methods.

This procedure also insures a separate Const for each parameter. If a
Const is shared by other ugens, setting the *input* of one of those ugens
changes the input of other ugens. In that case, it is better to treat the
Const object as a shared variable with its own ID and set the Const object directly
using that ID.

If the current input is A-rate (audio) or B-rate (block rate), you can *replace*
the input (and also unref the current one) by using a replace command:
```
/arco/fmosc/repl_freq ID freqID
```
where freqID is the ID of the replacement input.

### More Details

In the case of `/arco/<class>/new`, if the input is a
"constant" (as opposed to audio-rate or block-rate signal,
"constant-rate" signals represent float values that can be set by
messages, so in effect, they are piece-wise constant signals), you
must create a constant object with `/arco/const/new` or
`/arco/const/newf` messages and then pass the ID of the constant
object as the input to the new object.

In the case of `/arco/<class>/set_<input>`, if the input is a
constant, then the constant value is actually stored in another
object of class `Const`. To perform `set_<input>`, the object will
follow a pointer to the `Const` object and modify it. In principle,
a single `Const` object can provide constant input values to
multiple objects, so `set_<input>` could potentially change the
value sent to multiple inputs. Whether this is a feature or a bug
depends on what model you prefer: Is a piece-wise constant signal
a real signal that can fan out to multiple objects, or is it just
a property of a single unit generator? In the current Serpent
library, we initialize objects with inputs that can be other unit
generator objects or simple floats. In the case of floats, we make
a hidden unique Const object for the unit generator, so this follows
the "property of a single ugen" model. But you can also create a
`Const` object and share it among multiple unit generators.

If a unit generator name ends with a star (`*`), it has two forms:
output at audio rate and output at block rate. The unit generator is
described in its audio rate form, but by appending `b`, you get the
block rate form. E.g. `/arco/mult/new` creates an audio-rate unit
generator and `/arco/multb/new` creates a block-rate unit generator.
There are some restrictions on types, but in general, the only
restriction is that you cannot set an input to a block-rate unit
generator to an audio-rate signal. Unless otherwise noted, inputs
to an audio-rate unit generator can be any mix of audio-rate,
block-rate, and "constant-rate" (class Const) signals.

## Unit Generators and Messages

In each section, Serpent constructor and methods are listed in **`bold`**
followed by `O2 message formats`.

### alpass

**`alpass(inp, dur, fb, maxdur [, chans])`**

`/arco/alpass/new id chans inp dur fb maxdur` - Create a new alpass unit
generator with audio input and output. All channels have the same maximum duration (`maxdur`), but each channel can have a different delay (`dur`) and feedback
(`fb`). `id` is the object id. This is based on `delay`, but uses an allpass filter
design where every frequency has the same gain. If the desired control parameters
are frequency (in Hz) and decay time (T60, in seconds), set `dur` to 1 / *hz* and set
`fb` (feedback) to `exp(-6.9087 * dur / decay_time)`

`/arco/delay/max id dur` - Reallocates delay memory for a maximum duration of `dur`.

`/arco/delay/repl_dur id dur_id` - Set duration input to object with id `dur_id`.

`/arco/delay/set_dur id chan dur` - Set duration to a float value `dur`.

`/arco/delay/repl_fb id fb_id` - Set feedback to object with id `fb_id`.

`/arco/delay/set_fb id chan fb` - Set feedback to float value `fb`.

### delay
**`delay(inp, dur, fb, maxdur [, chans])`**

`/arco/delay/new id chans inp dur fb maxdur` - Create a new feedback delay
generator with audio input and output. All channels have the same maximum duration (`maxdur`), but each channel can have a different delay (`dur`) and feedback
(`fb`). `id` is the object id.

`/arco/delay/max id dur` - Reallocates delay memory for a maximum duration of `dur`.

`/arco/delay/repl_dur id dur_id` - Set duration input to object with id `dur_id`.

`/arco/delay/set_dur id chan dur` - Set duration to a float value `dur`.

`/arco/delay/repl_fb id fb_id` - Set feedback to object with id `fb_id`.

`/arco/delay/set_fb id chan fb` - Set feedback to float value `fb`.

### feedback

**`feedback(inp, from, gain [, chans])`**

`/arco/feedback/new id chans inp from gain` - Create a cycle in the
graph of unit generators. Output from the unit generator specified
by `from` (an id for another ugen) will be scaled by `gain` and mixed
with `inp` to form the output. The signal from `from` will be delayed
by one block (default size is 32 samples) because up-to-date output
from `from` may depend on our output, creating a circular dependency.

The detailed operation is this: A buffer is initialized to zero. To
compute output from this feedback ugen, both `inp` and `gain`, but
not `from`, are updated so they hold current values. The buffer is
scaled by `gain` and added to the input (`inp`) to form the output.
Then, `from` is updated to hold current values (it may in fact
depend directly or indirectly on our output, which is now up-to-date).
The output of `from` is copied to our buffer to prepare for the next
block computation.

IMPORTANT:  If `from` depends directly or indirectly on the output of
this `feedback` object, a cycle is created, and Arco will be unable to
free any unit generators participating in the cycle. You can break the
cycle by replacing `from`. For example, before deleting this `feedback`
unit generator, you should probably replace `from` (see the `repl_from`
description below) with the built-in zero unit generator, which does
not depend on anything. Not that "freeing" or "deleting" a unit generator
simply removes any reference to it from a table that translates ids to
unit generators. This reduces the reference count on the unit generator,
but the memory is not freed unless the reference count goes to zero,
which cannot happen if there is a cycle.

`/arco/feedback/repl_inp id inp` - Set input to the object with id `inp`.

`/arco/feedback/repl_from id from` - Set the source of the feedback
(called "from") to the object with id `from`. This is especially useful
for setting `from` to the zero ugen, thus breaking any cycles, before
freeing the `feedback` ugen. (See "IMPORTANT" paragraph above.)

`/arco/feedback/repl_gain id gain` - Set gain signal to the object with id `gain`.

`/arco/feedback/set_gain id chan gain` - Set the gain for the given channel
(`chan`) to a the float value `gain`.

### fileplay

**`fileplay(filename, [chans], [start], [end], [cycle], [mix], [expand])`**
**`.go([playflag])`**
**`.stop()`**

Used to stream audio files from disk, fileplay uses a paired object
running in another thread to prefetch audio data into buffers. If
prefetching cannot keep up, output is filled with zeros until the
next buffer is available. There is currently no notification when
the first buffer is ready, but the playback does not start until
a `/arco/fileplay/play` message is sent. To detect when playback
is finished, a message is sent to `/actl/act` with the action id
provided earlier in `/arco/fileplay/act`.

`/arco/fileplay/new id chans filename start end cycle mix expand` - Create an
audiofile player with `chans` output channels, reading from `filename`,
starting at offset `start` (float in seconds), reading until offset
`end` (float in seconds), repeating an endless loop if `cycle` (Boolean)
is true, with audiofile channels mixed down to `chans` channels in
round-robin fashion when the file has more than `chans` files (if
`mix` (Boolean) is true) and omitting any channels above `chans` if
`mix` is false, and expanding input channels round-robin fashion to
`chans` output channels if the number of file channels is less than
`chans` (if `expand` is true), and padding channels with zero if the
file has fewer than `chans` channels. If `end` is zero, reading will
end at the end of the file.

`/arco/fileplay/play id play_flag` - Starts or stops playback according
to `playflag` (Boolean). Play will pause if necessary to wait for a
block of samples to be read from the file.

`/arco/fileplay/act id action_id` - Registers a request to send a
message to `/actl/act` with `action_id` (an integer greater than 0)
when file playback completes (or reaches `end`).

##filerec

**`filerec(filename, inp [, chans])`**
**`.go(rec_flag)`**
**`.stop()`**

The filerec unit generator writes its input to a file.

`/arco/filerec/new id chans filename inp` - Create an audiofile writer
that writes `chans` channels to `filename`. The source of audio is `inp`.

`/arco/filerec/repl_inp id inp` - Set the input to the object with id `inp`.

`/arco/filerec/act id action_id` - Set the action id to `action_id`.
This id is sent when the file is open and ready for writing.

`/arco/filerec/rec id record` - Start recording (when `record`, a Boolean)
is true. Stop recording when `record` is false. Recording can only be
started and then stopped, in that order. To record again, create a new
unit generator.


##flsyn

**`flsyn(path)`**
**`.alloff(chan)`**
**`.control_change(chan, num, val)`**
**`.channel_pressure(chan, val)`**
**`.key_pressure(chan, key, val)`**
**`.noteoff(chan, key)`**
**`.noteon(chan, key, vel)`**
**`.pitch_bend(chan, bend)`**
**`.pitch_sens(chan, val)`**
**`.program_change(chan, program)`**

Fluidsynth is a popular open-source sample-based synthesizer. `flsyn` is a
unit generator that uses the Fluidsynth engine to produce samples. This is
a MIDI synthesizer, so you control it by sending Arco messages that correspond
to MIDI messages.

*Note:* On macOS, Fluidsynth requires a dynamic library, `glib-2.0`, so the
resulting Arco application using `flsyn` is not complete or executable
without also providing the library. On the developer's machine, this is
```
/opt/homebrew/Cellar/glib/2.76.4/lib/libglib-2.0.dylib
```
Linking with the static version of this library results in some undefined
functions, so using the `.dylib` version is currently the only option.

`/arco/flsyn/new id path` - Create a unit generator based on Fluidsynth.
`path` (a string) is a file path to a sample library.

`/arco/flsyn/off id chan` - Performs a MIDI all-off operation on the given
channel. (`chan` is int32).

`/arco/flsyn/cc id chan num val` - Performs a MIDI control change operation
using channel `chan`, controller number `num`, and value `val`, all int32.

`/arco/flsyn/cp id chan val` - Performs a MIDI channel pressure (aftertouch)
operation using channel `chan` and value `val`, both int32.

`/arco/flsyn/kp id chan key val` - Performs a MIDI key pressure (polyphonic
aftertouch) operation using channel `chan`, key number `key`, and value
`val`, all int32.

`/arco/flsyn/noteoff id chan key` - Performs a MIDI note off operation
on channel `chan` and key number `key`, both int32. Note that there is no
velocity parameter.

`/arco/flsyn/noteon id chan key vel` - Performs a MIDI note on operation
on channel `chan` with key number `key` and velocity `vel`.

`/arco/flsyn/pbend id chan val` - Performs a MIDI pitch bend operation
on channel `chan`, an int32. The pitch bend is specified by a float from
-1 to +1. To get to the 14-bit pitch bend value, multiply `val` by
2^13, round to the nearest integer, add 2^13, and clip the value to the
range -(2^13) through (2^13)-1.

`/arco/flsyn/psens id chan val` - Set MIDI pitch bend sensitivity for
channel `chan` (int32). The `val` (int32) is the number of semitones of
the full-scale pitch bend. (The total pitch bend range is from `-val` to
`+val`.)

`/arco/flsyn/prog id chan program` - Performs a MIDI program change operation
using channel `chan` and program number `program` (both int32).

### granstream

**`granstream(inp, polyphony, dur, enable [, chans])`**
**`.set_polyphony(p)`**
**`.set_ratio(low, high)`**
**`.set_graindur(lowdur, highdur)`**
**`.set_density(density)`**
**`.set_env(attack, release)`**
**`.set_enable(enable)`**

`/arco/granstream/new id chans inp polyphony dur enable` - Create a new
granular synthesis from input audio unit generator with the given `id`
number and `chans` channels. The input is initially given by `inp` and
each output channel will be the sum of `polyphony` independently generated
grain sequences. Grains are taken from recent input with a buffer length
of `dur` seconds, and grains are produced when `enable` (Boolean) is true.

The algorithm is as follows: A set of `polyphony` grain generators is allocated
for each channel. Grain generators follow a sequence of states: Pick a random
delay based on `density`. Also pick a resampling ratio and a duration.
After the delay, pick a starting point in the buffer of previous input samples.
Read the grain starting at this point. Linearly ramp the grain envelope
from 0.0 up to 1.0 over the `attack` time. Hold the envelope at 1 for a
duration based on the grain duration. Then linearly ramp from 1.0 back to 0.0
over the `release` time. This complete one cycle of states. The cycle repeats
indefinitely. Note that each grain is produced with different random numbers,
so there is no synchronization among them or between channels.

`/arco/granstream/repl_inp id inp` - Set the input to the ugen with id `inp`.

`/arco/granstream/dur id dur` - Set the length of the buffer from which
grains are taken to `dur` (float) in seconds. Current buffer samples are
retained, and if `dur` increases, then the unknown samples (older than the
original value of `dur`) are set to zero.

`/arco/granstream/polyphony id polyphony` - Set the number of grain generators
per channel to `polyphony` (integer).

`/arco/granstream/ratio id low high` - Set the range of pitch shift applied
to grains to (`low`, `high`). Grains are pitch shifted by resampling with
linear interpolation. The `low` and `high` values are ratios where 1.0 means
no resampling, less than one implies downward pitch shifting, and greater than
one implies upward pitch shifting. For each grain, a ratio is randomly selected
from a uniform distribution over the given range. Ratio cannot be negative.

`/arco/grainstream/graindur id lowdur highdur` - Set the range of grain
durations to (`lowdur`, `highdur`). For each grain, a duration is randomly selected
from a uniform distribution over the given range.

`/arco/granstream/density id density` - Set the expected grain density to the
float value `density`. Density is the number of grains expected to be playing
at any given moment on any given channel. (Note that the apparent density may
be proportional to the number of channels, since 2 channels should have twice
as many grains as 1 channel.)

`/arco/granstream/env id attack release` - Set the attack and release times for
grains (times are in seconds).

`/arco/granstream/enable id enable` - Enable or disable the generation of grains
using the boolean value `enable`. When grains are not enabled, any currently
running grains (not in the delay state) are allowed to complete and ramp smoothly
to zero, so output can continue for up to the maximum grain duration after enable
is set to false.


### mix

**`mix([chans])`**
**`.ins(name, ugen, gain [, at_end])`** (name is a symbol)
**`.rem(name)`**
**`.find_name_of(ugen)`**
**`.set_gain(name, gain [, chan])`**

Mix accepts any number of inputs. Each input has a name and an associated gain,
which must be b-rate or c-rate. You can change the gain at any time. To change
the input signal, insert a new input and gain using the same name.

Multiple channels are handled as follows: First, each mixer input consists of
both an input signal and a gain. If both are single channel, we say the input
is single channel. If one is n-channel and the other (either input signal
or gain) is single *or* n-channel, we say the input is n-channel. The input
signal and gain are not permitted to have multiple channels that do not match,
e.g. n-channel and m-channel where n > 1, m > 1, and n != m. Attempts to insert
mismatched Ugens will have no effect other than printing a warning.

A single channel input signal will be duplicated n times if the gain has n
channels, and a single gain channel will be duplicated n times if the input
signal has n channels, forming n audio channels as the input signal. These
n channels are added to mixer output channels starting at channel 0 and
wrapping around if n is greater than the number of output channels.

Note that the number of channels in any given input does not need to match
the number of mixer output channels. Also note that a single input channel
is *never* expanded to the number of mixer channels. E.g., mono input is
routed to stereo left (only). To place a mono channel in the middle of a
stereo field, use a 2-channel gain, which could be as simple as a Const
Ugen with values [0.7, 0.7].

`/arco/mix/new id chans` - Create a new mixer with `chans` output channels.

`/arco/mix/ins id name input gain` - Insert an input to the mixer. The `name`
must not match the name of another input, or the existing input will be replaced.

`/arco/mix/rem id name` - Remove an input matching `name` from the mixer.

`/arco/repl_gain id name gain_id` - Set the gain for input `name` to
object with id `gain_id`.

`/arco/set_gain id name chan gain` - Set the gain for input `name` on
channel `chan` to the float value `gain`.


### mult, multb

**`mult(x1, x2 [, chans])`**

`/arco/mult/new id chans x1 x2` - Create a new multiplier.

`/arco/mult/repl_x1 id x1_id` - Set input x1 to object with id `x1_id`.

`/arco/mult/set_x1 id chan x1` - Set channel `chan` of input to float value `x1`.

`/arco/mult/repl_x2 id x2_id` - Set input x2 to object with id `x2_id`.

`/arco/mult/set_x2 id chan x2` - Set channel `chan` of input to float value `x2`.

The `multb` messages begin with `/arco/multb` and output is b-rate.


### olapitchshift

**`olapitchshift(inp, ratio, xfade, windur [, chans])`**
**`.set_ratio(value)`**

`/arco/olaps/new id chans inp ratio xfade windur` - Create a new
overlap-add pitch shifter with id given by `id`, `chans` channels,
and input with id `inp`, a pitch shift ratio of `ratio`, a cross-fade
time of `xfade`, and a window size of `windur`. The algorithm constructs
grains from the input of duration `windur` and resampled by `ratio` using
linear interpolation, and which overlap by `xfade`, with linear
interpolation of signals during the cross-fade time. When ratio is greater
than 1, pitch is scaled upward by `ratio`, and pitch is shifted downward
when `ratio` is less than one. All channels are processed synchronously
with the same window timing and pitch shift amount.

`/arco/olaps/ratio id ratio` - Set the pitch shift ratio to `ratio` (float).

`/arco/olaps/xfade id xfade` - Set the cross-fade time to `xfade` (float).

`/arco/olaps/windur id windur` - Set the window duration to `windur` (float).

`/arco/granstream/repl_inp id inp` - Set the input to the ugen with id `inp`.

## probe

**`probe(inp, reply_addr)`**
**`.probe(period, frames, chan, nchans, stride)`**
**`.thrsh(threshold, direction, max_wait)`**
**`.stop()`**

A `probe` is used to send samples from the audio server to an O2 service.
In particular, the oscilloscope display uses `probe` to obtain samples to display.

`/arco/probe/new id input_id reply_address` - create a `probe` with the given
`id`, initially with input from `input_id` and send samples to `reply_address`.
Note that `chans` is not specified, but see below about how channels are handled.

`/arco/probe/repl_inp id input_id` - replace input.

`/arco/probe/probe id period frames chan nchans stride repeats` - probes the input
signal, sending samples to the `reply_address` given in the `new` message.
When a `probe` message is received, any previous operation is terminated
and the object is reinitialized to respond immediately to this probe request.
 - `id` indicates the probe to update.
 -  `period` is the period in seconds (type float) from one probe to the
    next, i.e., the hop size. Send -1 for a one-shot probe. For any
    period >= 0, periods are measured relative to the
    *first* frame of a message, but actual period is at least
    frames * stride (no overlapping frames) rounded up to the
    nearest block, and if thresholding
    is enabled, the search for a threshold *begins* after the
    period has elapsed.
 - `frames` is the number of frames collected (roughly) each `period`.
   The limit for `frames * nchans` is 64, but see `repeats` below. If
   `frames * nchan` > 64, `frames` is reset to `64 / nchans` (which is
   unfortunately 0 if `nchans` > 64!)
 - `chan` is the first channel to collect. If `chan` is out of range for
   the input, 0 is used.
 - `nchans` is the number of contiguous channels to collect.
   If the given range is outside the available channels, `nchans` is reduced
   as much as needed. E.g. `chan` = 0 and `nchans` = 999 will probe all channels
   (or up to 999 of them).
 - `stride` is normally 1, but you can downsample the signal by collecting
   every `stride` frames with `stride` > 1. If `stride` < 1, 1 is used. Long
   strides are possible and generate printed warnings.
 - `repeats` gives the number of times to collect and send
   `frames * nchans` samples. E.g., to capture 512 successive samples
   from one channel, set `frames` = 64, `repeats` = 8. If `repeats` < 1,
   1 is used.

`/arco/probe/thresh id threshold direction max_wait` - To wait for a
threshold crossing (e.g. for an oscilloscope), set `threshold` to the
threshold value, and `direction` to 1 for positive and -1 for negative
direction, or 0 for no threshold detection (collect immediately).
Then send /arco/probe/probe as above. Use `max_wait` to control how
long to wait for a zero crossing:

      DESIRED BEHAVIOR        MAX_WAIT     DIRECTION
      wait until threshold    zero         non-zero
      wait up to max_wait     non-zero     non-zero
      do not wait             *            zero

`/arco/probe/stop id` - Stop collecting samples and send an empty message.
When an empty (no floats) message arrives, the probe has been reset.

To get periodic snapshots, set period to seconds; period
will be quantized to blocks, and period may not be uniform if
threshold detection is enabled

The O2 messages consists of the probe id (int32) followed by float samples
(up to 64 of them).

## pv

**`pv(inp, ratio, fftsize, hopsize, points, mode [, chans])`**
**`.set_ratio(value)`**
**`.set_stretch(value)`**

The `pv` unit generator uses a phase vocoder to implement time stretching
and pitch shifting. Time stretching is a standard function of the phase
vocoder. Pitch shifting is obtained by stretching the signal (which
changes the duration but not the pitch) and then resampling to undo the
time stretch (which *does* change the pitch), resulting in pitch shift
without stretch.

You cannot stretch sounds that are computed in real time since there is
no place to store samples that arrive faster than needed, and of course
you cannot read samples from the future if you want to play a sound
faster than real time. However, you can obtain samples from `recplay` or
`playfile` to stretch pre-recorded sounds. For this to work, it is
imperative that the only consumer of samples is a `pv` unit generator.

`/arco/pv/new id chans inp ratio fftsize hopsize points mode` - Create a
phase vocoder unit generator with `chans` channels and input `inp`.
All channels use the same control parameters so processing is
synchronous across all channels. Pitch is shifted by `ratio`
(use a ratio greater than 1.0 to shift higher). The `fftsize` should
be a power of 2. 2048 is a good choice. The `hopsize` refers to the
overlap (in samples) and should normally be `fftsize / 8`. The
smallest  `ratio` is `3 * hopsize / fftsize` so smaller hop sizes
may be needed for extreme downward pitch shifts. Resampling uses
sinc interpolation over `points` samples. The `mode` is 0
for normal, 1 for a phase computation that attempts to reduce phase
artifacts by preserving the phase relationships between peaks and nearby
bins, and 2 invokes a "robot voice" mode that assigns fixed phases
and creates a constant pitch (controlled by the hopsize) vocoder-like
effect. (Thanks to M.C.Sharma for this idea.)

`/arco/pv/stretch id stretch` - Sets the stretch factor, which is 1.0
by default. Time stretching can only work with non-real-time input
provided by `fileplay` or `recplay` unit generators. Both pitch shifting
and time stretching can be applied together.

`arco/pv/ratio` id ratio - Sets the pitch shift in terms of frequency
ratio. Greater than 1 means raise the pitch.

`arco/pv/repl_inp id inp_id` - Set the input to the object with id `inp_id`.

### pwe, pweb

**`pwe(d0, y0, d1, y1, ..., [init=0], [start=true])`**
**`.set_points(d0, y0, d1, y1, ...)`**
**`.start()`**
**`.decay(dur)`**
**`.set(y)`**

These envelope generators generate approximately exponential curves between
positive breakpoints. To avoid the problem of zero (exponential decays never
actually reach zero), all breakpoint values are increased by 0.01. The
exponential interpolation occurs between these biased breakpoints. Then,
0.01 is subtracted to obtain the output. This allows envelopes to decay all
the way to zero, but for very small breakpoint values, the curves are close
to linear.

`/arco/pwe/new id` - Create a new piece-wise linear generator with audio
output. `id` is the object id. Envelope does not start until `start`
or `decay` is sent.

`/arco/pwe/env id d0 y0 d1 y1 ... dn-1 [yn-1]` - set the envelope or function shape for object with id. All remaining parameters are floats, alternating segment durations (in samples) and segment final values. The envelope starts at the current output value and ends at yn-1 (defaults to 0).

`/arco/pwe/start id` - starts object with id.

`/arco/pwe/decay id dur` - decay from the current value of object with id to zero in `dur` (int32) blocks.

`/arco/pwe/set id y` - sets current output value to `y` (float). If the unit
generator is in the middle of an envelope, this will create a discontinuity
and may result in strange output because the output will continue to increase
or decrease exponentially from `y`.

If the case of **pweb**, output is b-rate, and all messages are the same
except the address begins with `/arco/pweb/`.


### pwl, pwlb

**`pwl(d0, y0, d1, y1, ..., [init=0], [start=true])`**
**`.set_points(d0, y0, d1, y1, ...)`**
**`.start()`**
**`.decay(dur)`**
**`.set(y)`**


`/arco/pwl/new id` - Create a new piece-wise linear generator with audio output. `id` is the object id.

`/arco/pwl/env id d0 y0 d1 y1 ... dn-1 [yn-1]` - set the envelope or function shape for object with id. All remaining parameters are floats, alternating segment durations (in samples) and segment final values. The envelope starts at the current output value and ends at yn-1 (defaults to 0).

`/arco/pwl/start id` - starts object with id.

`/arco/pwl/decay id dur` - decay from the current value of object with id to zero in `dur` samples.

`/arco/pwl/set id y` - sets current output value to `y` (float). If the unit
generator is in the middle of an envelope, this will create a discontinuity
and may result in strange output because the output may continue to increase
or decrease from `y`.

If the case of **pwlb**, output is b-rate, and all messages are the same
except the address begins with `/arco/pwlb/`.

### recplay

**`recplay(inp, [chans], [gain], [fade_time], [loop])`**
**`.record(record_flag)`**
**`.start(start_time)`**
**`.stop()`**

`recplay` is a unit generator that can record sound and play it back. It applies a smooth envelope when the playback starts and stops, and it can automatically loop the recorded sound.

`/arco/recplay/new id chans inp_id gain_id fade loop` - Create a `recplay` ugen.
`chans` is the number of channels recorded and played (the channels are all
synchronous). `gain_id` is normally a c-rate ugen. Each channel has independent
gain, but gain must be c-rate or b-rate (not audio), and b-rate gain is not
interpolated. The intention is to implement efficient adjustable gains that
remain fixed. Use a mixer or multiplier for amplitude modulation. `fade` is
the fade time (a float, applies to all channels). The fade is a raised
cosine curve and is applied to both the beginning and ending of the recorded
signal during playback. `loop` is a boolean.
When `loop` is true, playback restarts immediately when the end of the recording
is reached while playing.

`/arco/recplay/repl_inp id inp_id` - Set the input to the object with
id `inp_id`.

`/arco/recplay/repl_gain id gain_id` - Set the gain to the object with
id `gain_id`.

`/arco/recplay/set_gain id chan gain` - Set the gain of channel `chan` to
`gain` (a float). The gain input must be a `const` ugen.

`/arco/recplay/speed id speed` - Set the playback speed to (float) `speed`.
The same speed applies to all channels. Samples are linearly interpolated,
and `speed` is a relative value, nominally 1.0.

`/arco/recplay/rec id record` - Start or stop recording. `record` is a
boolean value (true to start). When you start to record, any previous
samples recorded are deleted.

`/arco/recplay/start id start_time` - Start playback from time offset
`start_time` (a float), which is relative to the start of recording.

`/arco/recplay/act id action_id` - Set the action id to `action_id`.
This id is sent when playback reaches the end of the recording.

`/arco/recplay/stop id` - Stop playback. Playback continues until the
fade-out completes. (Note that the fade-out may have already started
if the end of recording is near.)

` /arco/recplay/borrow id lender_id` - A single recording can be played
by multiple `recplay` ugens at the same time. To do this, use one `recplay`
to make the recording. Then, "borrow" the recording by sending this message
to another `recplay` object. The borrower should not record anything, but it
can play the recording. Reference counting is used so that recordings are
only freed when no `recplay` has a reference.


### reson

**`reson(inp, center, q [, chans])`**

`/arco/reson/new id chans center bandwidth` - Create a new reson filte with `center` frequency and `bandwidth` control inputs, audio input and audio output.

`/arco/reson/repl_center id center_id` - Set center frequency to object with id `center_id`.

`/arco/reson/set_center id chan center_id` - Set center frequency of channel
`chan` to float value `center_id`.

`/arco/reson/repl_bandwidth id bandwidth_id` - Set bandwidth to object with id `bandwidth_id`.

`/arco/reson/set_bandwidth id chan bandwidth_id` - Set bandwidth of channel
`chan` to float value `bandwidth_id`.


### sine, sineb

**`sine(freq, amp [, chans])`**

`/arco/sine/new id chans freq amp` - Create a new sine oscillator.

`/arco/sine/repl_freq id freq_id' - Set frequency to object with id `freq_id`.

`/arco/sine/set_freq id chan freq` - Set frequency of channel `chan` to 
float value `freq`.

`/arco/sine/repl_amp id amp_id' - Set amplitude to object with id `freq_id`.

`/arco/sine/set_amp id chan amp` - Set amplitude of channel `chan` to 
float value `amp`.

The `sineb` unit generator addresses begin with `/arco/sineb` and the output
is b-rate.

### thru, fanout

**`Thru(inp [, chans] [, id_num])`**
**`thru(inp [, chans])`**
**`fanout(inp, chans)`**
**`.set_alternate(alt)`**

The Thru unit generator passes input to output without modification or delay.
One application is to expand 1 channel to n channels, taking advantage of
Arco's standard channel-adapting feature where single channel inputs are
copied to form the required number of channels. Since this is a built-in
fanout policy, you do not normally need `fanout` unless you want to force
the number of channels or route  mono signal to all output channels (audio
output is an exception in that if you play n channels which is fewer than
exist in the output, only the first n output channels will receive a signal.)

Thru objects are also used for audio input (the audio input is written
directly into the thru object's outputs and `inp_id` is ignored) and to
make the audio output available with a one-block delay (after computing the
output, it is written to the thru object's outputs and `inp_id` is ignored.)
These special thru objects have index `INPUT_ID` (= 2) and
`PREV_OUTPUT_ID` (= 3).

`/arco/thru/new id chans inp_id` - Create a pass-through object with input
`inp_id`. Audio is copied from `inp_id` to the output without modification
except that 1-channel inputs are expanded to `chans`.

`/arco/thru/alt id alt_id` - When a unit generator requests output from
this thru object (`id`), it will be redirected to the output of `alt_id`.
The object there must have a compatible number of channels (either equal,
or `alt_id` has a single channel.)
To remove the alternate and resume taking input from the thru object's
input, pass ZERO_ID as `alt_id`. To make the thru's output zero, e.g., to
mute the output of `INPUT_ID`, create a new `zero` object and pass it as
`alt_id`. The `alt_id` capability allows audio input (a Thru object) to
be redirected to obtain input from a file or test signal.


### vu

**`vu(reply_addr, period)`**
**`.start(replay_addr, period)`**
**`.set('inp', ugen)`**

`/arco/vu/new id reply_addr period` - Create a vu object, which measures the peak
values of input channels over every `period` (float, in seconds) and sends the
values as floats to `reply_addr` (string representing a complete O2 address).
No messages are sent until input is set with `repl_inp`:

`/arco/vu/repl_inp id inp_id` - Set the input to be analyzed to `inp_id`. If
`inp_id` names a unit generator of class Zero, analysis is stopped; otherwise,
processing will start or resume, sending a peak every period.

`/arco/vu/start id reply_addr period` - Change the `reply_addr` and `period`
of this vu object.


## yin

`yin(inp, minstep, maxstep, hopsize, address, [chans]))`
`.thresh(x)`
`.set('inp', ugen)`

The `yin` unit generator implements the YIN algorithm for pitch estimation.

`/arco/yin/new id chans inp_id minstep maxstep hopsize address` -
Creates a YIN pitch estimating unit generator that processes `chans`
channels of input audio. The algorithm searches for a fundamental
frequency between `minstep` and `maxstep` (both integers expressed as
MIDI key numbers), performing an analysis every `hopsize` samples.
The best candidate, its "harmonicity," and RMS are sent (as floats) to
`address`, an O2 address. If there are multiple channels, a single
message is sent with triples of results: frequency, harmonicity, rms
for channel 0, then frequency, harmonicity, rms for channel 1, etc.
Fundamental frequency is reported in units corresponding to MIDI
key numbers, but floats are used to give a continuous scale.
Harmonicity might be better labeled "aperiodicity" because it measures
a relative difference between two successive periods. The number
varies from 0 to 1, and small numbers indicate confidence in the pitch
estimate. RMS is the root-mean-square of the signal measured over two
periods of `minstep` (this is a longer window than two periods at the
reported frequency.)

`/arco/yin/repl_inp id inp_id` - Set the input to be analyzed to `inp_id`.
If `inp_id` names a unit generator of class Zero, analysis is stopped;
otherwise, processing will start or resume.


### zero, zerob

**`zero()`** (Note that this always returns the unique system zero ugen.)

`/arco/zero/new id` - Create a signal consisting of all-zeros.

