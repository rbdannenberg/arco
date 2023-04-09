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

## Unit Generators and Messages

### mult
`/arco/mult/new id chans x1 x2` - Create a new multiplier.

`/arco/mult/repl_x1 id x1_id` - Set input x1 to object with id `x1_id`.

`/arco/mult/set_x1 id chan x1` - Set channel `chan` of input to float value `x1`.

`/arco/mult/repl_x2 id x2_id` - Set input x2 to object with id `x2_id`.

`/arco/mult/set_x2 id chan x2` - Set channel `chan` of input to float value `x2`.

### mix
`/arco/mix/new id chans` - Create a new mixer with `chans` output channels.

`/arco/mix/ins id input gain` - Insert an input to the mixer.

`/arco/mix/rem id input` - Remove an input from the mixer.

`/arco/repl_gain id inp gain_id` - Set the gain for input `inp` to
object with id `gain_id`.

`/arco/set_gain id inp gain` - Set the gain for input `inp` to the
float value `gain`.


### sine
`/arco/sine/new id chans freq amp` - Create a new sine oscillator.

`/arco/sine/repl_freq id freq_id' - Set frequency to object with id `freq_id`.

`/arco/sine/set_freq id chan freq` - Set frequency of channel `chan` to 
float value `freq`.

`/arco/sine/repl_amp id amp_id' - Set amplitude to object with id `freq_id`.

`/arco/sine/set_amp id chan amp` - Set amplitude of channel `chan` to 
float value `amp`.

### pwl
`/arco/pwl/new id` - Create a new piece-wise linear generator with audio output. `id` is the object id.

`/arco/pwl/env id d0 y0 d1 y1 ... dn-1 [yn-1]` - set the envelope or function shape for object with id. All remaining parameters are floats, alternating segment durations (in samples) and segment final values. The envelope starts at the current output value and ends at yn-1 (defaults to 0).

`/arco/pwl/start id` - starts object with id.

`/arco/pwl/decay id dur` - decay from the current value of object with id to zero in `dur` samples.

### delay
`/arco/delay/new id chans inp dur fb maxdur` - Create a new delay generator with audio input and output. `id` is the object id. `maxdur` is the maximum duration (this much space is allocated).

`/arco/delay/set_max id dur` - Reallocates delay memory for a maximum duration of `dur`.

`/arco/delay/repl_dur id dur_id` - Set duration input to object with id `dur_id`.

`/arco/delay/set_dur id dur` - Set duration to a float value `dur`.

`/arco/delay/repl_fb id fb_id` - Set feedback to object with id `fb_id`.

`/arco/delay/set_fb id fb` - Set feedback to float value `fb`.


### reson
`/arco/reson/new id chans center bandwidth` - Create a new reson filte with `center` frequency and `bandwidth` control inputs, audio input and audio output.

`/arco/reson/repl_center id center_id` - Set center frequency to object with id `center_id`.

`/arco/reson/set_center id chan center_id` - Set center frequency of channel
`chan` to float value `center_id`.

`/arco/reson/repl_bandwidth id bandwidth_id` - Set bandwidth to object with id `bandwidth_id`.

`/arco/reson/set_bandwidth id chan bandwidth_id` - Set bandwidth of channel
`chan` to float value `bandwidth_id`.
