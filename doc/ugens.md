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

However, if the Const is shared by other ugens, setting the *input* of one of those ugens changes the input of other ugens. In that case, it is better to treat the Const object as a shared variable and set the Const object directly.

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

### delay
`/arco/delay/new id chans inp dur fb maxdur` - Create a new delay
generator with audio input and output. `id` is the object id.
`maxdur` is the maximum duration (this much space is allocated).

`/arco/delay/set_max id dur` - Reallocates delay memory for a maximum duration of `dur`.

`/arco/delay/repl_dur id dur_id` - Set duration input to object with id `dur_id`.

`/arco/delay/set_dur id dur` - Set duration to a float value `dur`.

`/arco/delay/repl_fb id fb_id` - Set feedback to object with id `fb_id`.

`/arco/delay/set_fb id fb` - Set feedback to float value `fb`.


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


### mix
`/arco/mix/new id chans` - Create a new mixer with `chans` output channels.

`/arco/mix/ins id input gain` - Insert an input to the mixer.

`/arco/mix/rem id input` - Remove an input from the mixer.

`/arco/repl_gain id inp gain_id` - Set the gain for input `inp` to
object with id `gain_id`.

`/arco/set_gain id inp gain` - Set the gain for input `inp` to the
float value `gain`.


### mult*
`/arco/mult/new id chans x1 x2` - Create a new multiplier.

`/arco/mult/repl_x1 id x1_id` - Set input x1 to object with id `x1_id`.

`/arco/mult/set_x1 id chan x1` - Set channel `chan` of input to float value `x1`.

`/arco/mult/repl_x2 id x2_id` - Set input x2 to object with id `x2_id`.

`/arco/mult/set_x2 id chan x2` - Set channel `chan` of input to float value `x2`.


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

