# Arco Instruments and Effects

**Roger B. Dannenberg**

## Introduction
Beyond unit generators (see [ugens.md](ugens.md)), there are
instruments and effects constructed from unit generators and available
as Serpent files, described here.

## Requirements
Instruments are classes that can be instantiated. They have multiple
parameters. A parameter can be represented by a Const_like object or a
method such as 'set_modulation'. It would be simpler if every
parameter was simply an Arco Const_like ugen, but there are cases
where the computation and math makes it awkward to simply set a value
in a Ugen, so we need methods as a more general way to change a
parameter.

Instruments can be expensive to build, so we use a Synth instance to
manage pre-built instrument "resources" to play notes. Parameters are
shared by default, meaning that by setting the parameter in the Synth,
all instrument instances managed by the Synth will update their local
parameters. But instruments can also have customized parameter values
that override the Synth parameters.

It is not flexible enough to designate some parameters as
note-specific and others as shared, so we really need a parameter
override mechanism.

One attractive way to share parameters is to connect a Const_like Ugen
to every instrument. E.g. if every instrument has a multiplier with a
gain input, one would normally create a Const or Smoothb for the gain
value and there would be one gain Ugen per instrument. But it's
possible to have a single shared gain Ugen that provides the input for
*every* instrument. Then, you just need to set gain with a single
message to affect all instruments, including those that may not even
be allocated or active until later.

Unfortunately, this does not work when parameter updates are
implemented by methods or when instruments want to override a shared
variable, which requires a lot of extra care. To override a shared
Ugen, you have to:
- make a new Const_like Ugen
- know which inputs of the particular instance of the instrument get
  their value from this parameter
- send "_repl" messages to replace each input with the new
  instrument-specific Const_like Ugen
- keep track of which parameters are now instrument-specific
- when the instrument is reallocated, replace instrument-specific
  parameters with shared parameters held by the synth as appropriate
  
Because of these complications, we will just send Synth-level updates
to all active instruments. We also keep a cache of Synth-level
parameter values and apply them when an instrument is reallocated.

## Instrument Parameters
To set instrument parameters, we extend the Instrument mechanisms with
a more general way to define and update instrument parameters. The
main principle is that both primitive Ugens and constructed
Instruments have the same interface: `set(attribute, value)`.

### From Instrument Attribute to Ugen Attribute
Thus, `Instrument.set(atttribute, value)` must have some kind of map
from Instrument attribute to component and component
attribute. Instruments will all have a map with entries of the form:

>   attribute -> (component, attribute)

The syntax we would like to use names the Instrument attribute at the
site where the component is initialized. E.g. within an instrument
that uses a Sine Ugen, we might write
```
    s = Sine(freq, Param('gain', 0.5))
```
after which `instrument.set('gain', 0.6)` will invoke `s.set('amp', 0.6)`
since the second parameter to `Sine()` is named `'amp'`. The trick to
implement this is that `Param` will register an incomplete mapping for
the instrument and the `Ugen()` initialization, used by all Ugen
subclasses, will fill in the mapping with the Sine instance and the
parameter name.

### From Instrument Attribute to Instrument Method
For completeness, Instruments need at least one alternate mapping of
the form:

>    attribute -> (component, method)

so that the instrument can do custom processing by invoking a method,
e.g. setting `'wet'` might set one gain to x and another to 1-x.

If the attribute is not a float, the method will get a float array or
Ugen as its parameter. It is up to the Instrument to at least warn the
user if it can only handle floats.

### Clipping, Scaling, Smoothing

Other options, when we call Param, are:
  - clipping the value to lower and upper bounds
  - possibly mapping the range 0 to 1 to the range from lower to upper
    bound (independent of clipping)
  - Using Smoothb instead of Const for float parameters
  - Including a time constant for Smoothb
  
Clipping and Scaling apply even when a method is invoked. Smoothing
can *only* apply when a parameter is mapped to attributes of
components.

Clipping and Scaling can *only* apply to float updates. If you want to
replace a Const-like input with a non-Const-like signal input, the
signal is connected directly to input(s) of Ugen(s) and no
conditioning is applied. This could be very confusing, so for now,
this is not allowed. In the future, it might make sense to have a
Conditionerb Ugen that takes a signal input and applies scaling and
clipping and then run Instrument signal inputs through a Conditionerb
which would then be connected to Ugen inputs just like is done with
Const or Smoothb.

### Fanout

Within an instrument, a `Param` can be passed to multiple Ugens, in
which case the mapping must contain *multiple* targets:

> attribute -> [(component, attribute), (component, attribute), ...])

Fanout is limited:
  1. The same scaling, clipping and smoothing is shared by all
     consumers of the parameter value.
  2. The consumers must all be Ugen inputs, in which case a Const-like
     Ugen is constructed and shared by all inputs, or all methods,
     in which case each method is called with a float value.

To work around these limitations, you can always just map the
attribute to a single Instrument method, which can perform any
computation on incoming float, array, and Ugen parameter values and
control any number of sub-instrument and Ugen components.

### Subinstruments

Subinstruments are not a special class, but simply Instruments that
are used as components of another Instrument. Subinstruments and the
approach to fanout present a problem: Scaling, clipping and smoothing
happen at the instrument level and floats are converted to Const-like
Ugens that are connected to components of the instrument. But if a
subinstrument *also* want to apply scaling or smoothing, it needs to
see the original float value.

Therefore, to implement `set(attribute, value)` at the Instrument
level, we need to treat subinstruments separately from Ugens. For
subinstruments, we apply conditioning (scaling and clipping) and then
simply invoke the subinstrument's `set` method to possibly do further
scaling and clipping.


### Setting Different Types of Values

Also, the type of value in set(attribute, value) can be:
  - float - if the prior value was a float, map attribute to a Ugen's 
    <attr> and send `.../set_<attr>` to the Ugen on channel 0. Otherwise, 
    create a Const-like Ugen and `.../repl_<attr>` to the created Ugen.
  - array of float - if the prior value was a float, map attribute to 
    a Ugen's <attr> and send `.../set_<attr>` to the Ugen for each 
    channel. Otherwise, create a multi-channel Const-like Ugen and 
    `.../repl_<attr>` to the created Ugen.
  - Ugen - map attribute to a Ugen's <attr> and send
    `.../repl_<attr>` to the Ugen.

Therefore, the values for Param can be the same: float, array of
float, or Ugen, which creates Const-like Ugens as needed.

The Parameter descriptor, or Param_descr, has these fields:
  - `name` - the symbol name of the parameter
  - `ugen_users` - a `Pair` or list of `Pair` consisting of:
    - `x` - the Ugen (or Instrument) that receives updated values.
    - `y` - the attribute of the target to set
  - `methods` - a `Pair` or list of `Pair` consisting of:
    - `x` - the Ugen (or Instrument) that receives updated values.
    - `y` - a method to invoke on `first` passing the value.
  - `subinstrs` - a `Pair` or list of `Pair` consisting of:
    - `x` - an Instrument (a subinstrument component to which
      values are passed)
    - `y` - the attribute of the subinstrument to set.
  - `smooth` - Pair of Smoothb and cutoff for smoothing iff floats
      should be directed through a Smoothb
  - `conditioner` - a Conditioner consisting low, high (clipping values)
      and operation ('clip', 'map' and 'mapclip' for clipping and mapping)
  - `value` - the most recent value, used to determine if/when to create 
      a Const-like Ugen and to filter redundant values. If the value
      is a Const-like Ugen and the parameter is a float or array of
      floats, we send the updates to the Const-like Ugen, which is
      already attached to every `Param_target`.

### Implementation Note on Const-like Parameters

"Constant" Ugen parameters are actually block-rate signals coming from
a Const Ugen or a Smoothb Ugen, both of which are controlled by
setting them to float values, which they then output. (Although
Smoothb output is more signal-like in that transitions to new values
are smoothed.)

There are two ways to update a Ugen input with a float:
  1. Direct: Send a `set` message to the Const-like Ugen that is
     connected to one of the Ugen's signal inputs. This requires
     keeping track of the Const-like Ugen's ID, which differs from the
     ID of the Ugen you are ultimately affecting.
  2. Via Ugen: Send a `set_<attr>` message to the Ugen. Since the Ugen
     knows where the input for its `<attr>` parameter comes from, it
     can call the `Const:set` method for that input. This allows the
     creator of the Ugen to update Ugen parameter values without
     keeping track of all the Consts used to provide those
     parameters. (But this does not work for Smoothb: the input *must*
     be a Const, and this is checked in the code before coercing the
     input pointer from `Ugen` to `Const`.)
     
The Via Ugen method is used for primitive Ugens since it allows the
control code to just keep track of a single Ugen ID, but for
Instruments, there are two disadvantages:
  1. If the parameter is smoothed (a nice option), you cannot use the
     Direct method.
  2. If there is fanout, you can use the same Const-like Ugen for all
     Ugen inputs, which means you can update a single Const-like Ugen
     with a single message and the value will propagate (fan out) to
     all inputs, reducing message traffic and computation.
     
Therefore, Param_descr uses the Direct method and retains the ID of
the Const-like Ugen when there is one. When there is fanout, the first
Ugen that uses the parameter,

## Instrument Implementation
See [design.md](./design.md) for notes on how to implement an Instrument subclass.

#### * old stuff (ignore)*
*Instruments*

*Instruments have a dictionary called `parameter_bindings` that maps*
*parameters by name to a description of how to update them. The*
*description has 3 forms:*
*- the parameter value can refer to a component subinstrument or ugen.*
  *Calling*
*- the parameter value can be mirrored in a Const\_like ugen that*
  *can feed into and control multiple ugens that implement the instrument.*
*- the parameter value, when changed, can invoke a method of the*
  *instrument, allowing computation to be performed on the value*
  *before updating Arco ugens.*
*- the parameter may be passed directly to another instrument, allowing*
  *an instrument to be composed from one or more sub-instruments, where*
  *instrument parameters are "inherited" by sub-instruments. (More*
  *generally, there can be any sort of mapping from parent instrument*
  *parameters to sub-instrument parameters by invoking a method to do*
  *the mapping (see previous item on parameter methods).*
  
## Synthesizers and the Synth class

A Synth is a class that can "play" notes into a mixer. The Synth
abstraction lets you control and process a stream of "notes". A Synth
can be mono or stereo, and if stereo, notes can be panned.

**Important**: normally, when you subclass Instrument to make a
particular Instrument, you also subclass Synth to make a controller
for a group of such instruments. A useful convention is to use
`_synth` and `_instr` as suffixes, e.g.
```
class Supersaw_instr (Instrument):
    ...

class Supersaw_synth (Synth):
    def instr_create(note_spec, pitch, vel):
        Supersaw_instr(this, note_spec, pitch, vel)

    def init(instr_spec, dictionary_customization):
        super.init(instr_spec, customization,
                   ['chans', 'animate', 'detune', 'attack', 'decay'])
```
This example shows a minimal but typical complete definition of
`Supersaw_synth`, where the only real work is to connect
`instr_create` to the Supersaw instrument by constructing a
`Supersaw_instr` and creating an array with the complete list of
parameter names used by `Supersaw_instr`.
 
A Synth is associated with a particular Instrument Specification which
implies particular Instrument subclass, so you only instantiate
subclasses of Synth, and rather than instantiating a subclass
directly, you construct a Synth by writing:
``` 
synth = synth_create(instr_spec, dictionary customization)
``` 
where `instr_spec` is a dictionary containing 'instr', a symbol naming
the Synth subclass (e.g. `'Supersaw_instr'` that is instantiated with
the given Instrument Specification dictionary. `customization` is
created from keyword parameters which override what is in instr_spec.
 
When the Instr finishes its sound, it must call 
`synth.is_finished(this)` to remove itself from the synth's mixer and
enable its reuse as a new note in the future. This is the default
behavior of Instrument when its `finish` method is called.
 
To play a note, you invoke:
``` 
synth.noteon(pitch, velocity, optional dur = nil,
           id = nil, [customizations])
```
 
If the Synth does not have a free instrument in its pool for reuse, a
new one must be created by passing the dictionary to the `instr_create` 
method along with other parameters:
``` 
ins = instr_create(instr_spec, pitch, velocity, customizations)
``` 
Here, pitch is an integer or float following the MIDI keynumber
convention, but allowing for non-integer values, and `velocity` is a
number from 1 to 127 as in MIDI.
  
If `dur` is not supplied, then at a later time, you can call
``` 
synth.noteoff(id)
``` 
with the originally supplied "id" (a symbol or integer rounded from
pitch) to end the note.
 
Keyword parameters may contain `pan`: a number from 0 to 127 for left
to right as in MIDI, where 64 is center. If `pan` is omitted, the
value of 'pan' from `instr_spec` is used, and otherwise 64. If
synth has one channel or the instrument has 2 channels, `pan` is
ignored. (We should implement stereo pan though.) `pan` should be
ignored by the instrument itself.
 
Keyword parameters may contain `gain`: a linear scale factor applied  
to the instrument output that defaults to 1.0.  If `gain` is also in  
`instr_spec`, the product is used. instr_spec[`gain`] should be  
implemented in the instrument itself.

*customizations* is a list of optional keyword parameters.
 
By convention:

 - `modulation` is a number from 0 to 127 as in MIDI. The
   value is interpreted by the instrument (but can be ignored).

 - `release` is a release time in seconds controlling the time
   it takes from `noteoff` until the sound is no longer heard.
   The shape and perception of the release are instrument-dependent.
   
 - other "universal" parameters will be defined in the future
 
To change parameters while an instrument plays, you can either send
per-note parameter changes or send parameter changes to the synth,
which will affect all sounding and future notes. For per-note changes,
you can simply retain the note created by the synth, which is returned
from a call to `synth.noteon()`. E.g.
``` 
my_note = synth.noteon(pC4, 100)
my_note.update('modulation', 90)
``` 
Alternatively, you can tell the synth to send a per-note update using
the note Id or pitch:
``` 
synth.noteon(pC4, 100)
synth.update_note(pC4, 'modulation', 90)
``` 
To update all notes with a modulation change, use the Synth's `update`
method:
``` 
synth.noteon(pC4, 100)
synth.update('modulation', 90)
``` 
Again, this updates the value of 'modulation' in the synth so it will
affect future notes (similar to a control change sent to a MIDI
channel) except ones where 'modulation' is provided as a
note-specific value.
 
The `instr_spec` is a mechanism to reduce the number of parameters one
needs to pass to `synth.note()` to make a sound. It also captures the
notion of a "patch" or MIDI "program" in that the same general
instrument design can be customized through many parameters, and the
parameters can be stored in dictionaries loaded into global
variables. For example, you can have a generic FM instrument and many
specific parameter sets, allowing you so write, simply,
``` 
synth = synth_create(FM_HORN_1).play()
synth.note(C4, LF, 0.7)
``` 
rather than passing a large number of envelope, frequency, and
modulation control parameters to every note.

 
## Instrument Parameter Inheritance
A fundamental design question is how parameters reach Instrs. We want
some sort of hierarchy to simplify use. The levels of hierarchy are:

- Instrument Design ("architecture" or "algorithm") - a configuration
  of unit generators, sometimes capable of making many different
  sounds. It is represented by a subclass of class Instrument.
  
- Instrument Specification - a collection of parameters that have been
  chosen to achieve a specific sound. E.g. a wavetable instrument
  might have a specification that creates a flute-like sound and
  another one to make a violin-like sound. A specification, most
  commonly called an `instr_spec`, is a dictionary mapping parameter
  names to values (currently always numbers). Included in the
  `instr_spec` is `instr` which maps to the *name* (symbol) of the
  instrument design, a subclass of Instrument.
  
- Customizations - a particular instrument instance may have
  customizing parameters such as taking vibrato away from a violin
  sound or giving a sharper attack or longer decay to a brass sound.
  
- Per-note Parameters - each instance of an instrument will be given a
  pitch, velocity, and possibly other parameters. These are similar to
  customizations but they are specific to a particular note. This
  specification is constructed and passed to an instrument's `init`
  method as `note_spec`. The `note_spec` is used to initialize the
  instrument, and set up `parameter_bindings`, an Instrument instance
  variable that maps parameter names to parameter descriptions that
  contain the current parameter value and information on how to update
  the parameter (described in the previous section).

  
### Parameter Inheritance Data Flow
- initially, every parameter is provided by the Synth along with a
  default value.

- when we instantiate the Synth with a specification, a Synth instance
  configuration `instr_spec` is constructed by overwriting the initial
  default values with any values provided in the configuration (a
  dictionary).
  
- The Synth instance is also provided with a "customization" parameter
  that overrides values in the `instr_spec` dictionary (which might
  be a constant value maintained in a library of sound designs, so we
  do not want to modify it).
  
- Updates to the Synth can change parameter values. Sometimes, these
  are sent to active notes to control them in real time, but like MIDI
  control changes, the changes are associated with the Synth and apply
  to future notes, so they change state in the Synth and apply to
  future notes as well.
  
- When a note is created with `noteon`, parameters can be
  specified. These override the current state of parameters in the
  Synth's `instr_spec` dictionary, but only for this particular note.
  
- After a note is created, `update_note()` can be used to update a 
  parameter in a specific active note. It is up to the user to avoid 
  updating the same parameter in both the Synth and in specific 
  notes. If you do this, the last update prevails.
  

## Note Naming, Retrigger and Unisons

Notes will be identified in two ways: If an Id (a symbol) is provided,
the instrument is named by the Id and only the Id. If there is no Id,
the pitch is rounded to an integer which serves as the Id.

Only one note is allowed to have a given Id (whether it is a symbol or
a pitch). Note that by using distinct symbol Ids such as 'vn1' and
'vn2', unisons are possible. If a noteon is sent to the Id of an
instrument that already exists, the default is to send a noteoff,
allocate a new instrument and start it as with an ordinary noteon.  If
the instrument's has_retrigger field is true, its retrigger method is
called instead of noteon() and no new instance is allocated. The
retrigger method gets a new pitch and velocity, and it can have
customizing parameters as well.

## Noteoff

When state updates are received by the Synth, they are forwarded to
any active notes and recorded in the Synth state. After noteoff, notes
are no longer considered active and do not receive further parameter
updates, but the instrument continues to make sound until it
finishes. At this point, we want to make the note resources available
for reuse.

We make it the instrument's responsibility to notify the Synth when it
becomes reusable. The Instr has a 'synth' instance variable. If it is
non-nil, the Instr calls synth.is_finished(this) as a notification,
and the synth removes the instrument from the mixer and puts it on a
free list.

## Efficient Instrument Parameter Updates 

A parameter update begins with a call to `update` or `update_note`,
e.g.
```
synth.update('modulation', 100)
```
This method changes its `instr_spec` dictionary's 'modulation' to 100.
The synth also invokes `update` on every active note, which leads to
messages to Arco to change unit generators.

Notes are played by calling the Synth's method:
```
synth.noteon(pitch, velocity, param1 = value1, param2 = value2, ...)
```
where params are options that override configuration values (but do
not change the `instr_spec`; they apply only to the new note).

If an Instr must be created to play the note, we *could* build it with
default values and invoke `update_note` to install the current
`instr_spec` values, but we do not know which values have changed
from defaults, so this is not very efficient. We prefer to build the
Instr with the correct parameter values. The following:
```
synth.instr_create(instr_spec, pitch, velocity, customization)
```
builds an instrument, where `instr_spec` is the parameter dictionary
for the Synth and customization is a dictionary of overriding values
that come from keyword parameters passed to the Synth's `noteon`
method.

The Synth obtains note parameters by copying its `instr_spec` to make
`note_spec` and then searching for any name in `param_names` (a list
of all parameter names) that is in `customization` and overwriting the
value in `note_spec`. This is passed to the Synth's
`instr_create`. There, parameter values can be retrieved from
`note_spec`, but this is done with `note_spec.get('name', default)` so
that a default value can be supplied for any parameter that is not in
`note_spec`.

Since only active notes receive updates and there are note-specific
updates, there is a problem reusing inactive notes. The process for
note reuse updates each parameter that is different than the desired
configuration. To detect changes to the `instr_spec`, the
`parameter_bindings` includes the value of every parameter that can be
updated. We simply compare the value there to the desired value
and update if they differ.


## Reverb
A simple reverberation effect, based on Schroeder reverb design, is in
`reverb.srp`. To create:
```
rvb = reverb(inp, rt60)
```
where `inp` is the reverb input signal (a-rate) and `rt60` is the
reverb time (a float, not a signal). Methods include:
```
rvb.set_inp(inp)
```
Set or replace the input with `inp`.
```
rvb.set_rt60(rt60)
```
Change the reverb time to `rt60`.

# Fluidsynth
Fluidsynth is a well-known library. The interface from Arco to
Fluidsynth is described here.

`/arco/flsyn/new id sfont` - create a Fluidsynth object. `id` is the
(int32) object id, `sfont` is the (string) path to the soundfont file.
Because a soundfont must be loaded, you should create the `flsyn`
object when the program is initialized even if it will not be used
until later.

`/arco/flsyn/off id chan` - perform an "all-off" action. All notes
are ended.

`/arco/flsyn/cc id chan num val` - perform a control change operation.

`/arco/flsyn/cp id chan val` - perform a channel pressure
operation.

`/arco/flsyn/kp id chan key val` - perform a key pressure operation.

`/arco/flsyn/noteoff id chan key` - perform a note off operation.

`/arco/flsyn/noteon id chan key vel` - perform a note on operation.

`/arco/flsyn/pbend id chan val` - perform a pitch bend operation.
val is in the range -1 to 8192/8193 and will be clipped if out of
range.

`/arco/flsyn/psens id val` - set the pitch bend sensitivity to an
(integer) range of semitones given by val.

`/arco/flsyn/prog id chan program` - set the MIDI program number.

