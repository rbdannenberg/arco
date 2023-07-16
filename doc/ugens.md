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

### alpass
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
`/arco/delay/new id chans inp dur fb maxdur` - Create a new feedback delay
generator with audio input and output. All channels have the same maximum duration (`maxdur`), but each channel can have a different delay (`dur`) and feedback
(`fb`). `id` is the object id.

`/arco/delay/max id dur` - Reallocates delay memory for a maximum duration of `dur`.

`/arco/delay/repl_dur id dur_id` - Set duration input to object with id `dur_id`.

`/arco/delay/set_dur id chan dur` - Set duration to a float value `dur`.

`/arco/delay/repl_fb id fb_id` - Set feedback to object with id `fb_id`.

`/arco/delay/set_fb id chan fb` - Set feedback to float value `fb`.

### feedback
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


### granstream

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


### mult*
`/arco/mult/new id chans x1 x2` - Create a new multiplier.

`/arco/mult/repl_x1 id x1_id` - Set input x1 to object with id `x1_id`.

`/arco/mult/set_x1 id chan x1` - Set channel `chan` of input to float value `x1`.

`/arco/mult/repl_x2 id x2_id` - Set input x2 to object with id `x2_id`.

`/arco/mult/set_x2 id chan x2` - Set channel `chan` of input to float value `x2`.


### olapitchshift
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


### playfile
Used to stream audio files from disk, playfile uses a paired object
running in another thread to prefetch audio data into buffers. If
prefetching cannot keep up, output is filled with zeros until the
next buffer is available. There is currently no notification when
the first buffer is ready, but the playback does not start until
a `/arco/playfile/play` message is sent. To detect when playback
is finished, a message is sent to `/actl/act` with the action id
provided earlier in `/arco/playfile/act`.

`/arco/playfile/new chans filename start end cycle mix expand` - Create an
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

`/arco/playfile/play id play_flag` - Starts or stops playback according
to `playflag` (Boolean). Play will pause if necessary to wait for a
block of samples to be read from the file.

`/arco/playfile/act id action_id` - Registers a request to send a
message to `/actl/act` with `action_id` (an integer greater than 0)
when file playback completes (or reaches `end`).


## probe
A `probe` is used to send samples from the audio server to an O2 service.
In particular, the oscilloscope display uses `probe` to obtain samples to display.

`/arco/probe/new id input_id reply_address` - create a `probe` with the given
`id`, initially with input from `input_id` and send samples to `reply_address`.
Note that `chans` is not specified, but see below about how channels are handled.

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

### pwl
`/arco/pwl/new id` - Create a new piece-wise linear generator with audio output. `id` is the object id.

`/arco/pwl/env id d0 y0 d1 y1 ... dn-1 [yn-1]` - set the envelope or function shape for object with id. All remaining parameters are floats, alternating segment durations (in samples) and segment final values. The envelope starts at the current output value and ends at yn-1 (defaults to 0).

`/arco/pwl/start id` - starts object with id.

`/arco/pwl/decay id dur` - decay from the current value of object with id to zero in `dur` samples.


### reson
`/arco/reson/new id chans center bandwidth` - Create a new reson filte with `center` frequency and `bandwidth` control inputs, audio input and audio output.

`/arco/reson/repl_center id center_id` - Set center frequency to object with id `center_id`.

`/arco/reson/set_center id chan center_id` - Set center frequency of channel
`chan` to float value `center_id`.

`/arco/reson/repl_bandwidth id bandwidth_id` - Set bandwidth to object with id `bandwidth_id`.

`/arco/reson/set_bandwidth id chan bandwidth_id` - Set bandwidth of channel
`chan` to float value `bandwidth_id`.


### sine
`/arco/sine/new id chans freq amp` - Create a new sine oscillator.

`/arco/sine/repl_freq id freq_id' - Set frequency to object with id `freq_id`.

`/arco/sine/set_freq id chan freq` - Set frequency of channel `chan` to 
float value `freq`.

`/arco/sine/repl_amp id amp_id' - Set amplitude to object with id `freq_id`.

`/arco/sine/set_amp id chan amp` - Set amplitude of channel `chan` to 
float value `amp`.

### thru
Thru objects are used for audio input (the audio input is written
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
or `alt_id` is single channel.)
To remove the alternate and resume taking input from the thru object's
input, pass ZERO_ID as `alt_id`. To make the thru's output zero, e.g., to
mute the output of `INPUT_ID`, create a new `zero` object and pass it as
`alt_id`.


### vu
`/arco/vu/new id reply_addr period` - Create a vu object, which measures the peak
values of input channels over every `period` (float, in seconds) and sends the
values as floats to `reply_addr` (string representing a complete O2 address).
No messages are sent until input is set with `repl_inp`:

`/arco/vu/repl_inp id inp_id` - Set the input to be analyzed to `inp_id`. If
`inp_id` names a unit generator of class Zero, analysis is stopped; otherwise,
processing will start or resume, sending a peak every period.

`/arco/vu/start id reply_addr period` - Change the `reply_addr` and `period`
of this vu object.


### zero*
`/arco/zero/new id` - Create a signal consisting of all-zeros.

