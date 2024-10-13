# Arco Instruments and Effects

**Roger B. Dannenberg**

## Introduction
Beyond unit generators (see [ugens.md](ugens.md)), there are
instruments and effects constructed from unit generators and available
as Serpent files, described here.

## Generic Instrument Interface
Instruments can have arbitrary parameters and methods. To "normalize"
instruments and make them behave more like MIDI instruments, where
an instrument is conventionally polyphonic and controlled mainly by
note-on and note-off messages, and where there are often parameters
associated more with the instrument than with its control, we have
some conventions for instrument interfaces.

A generic instrument *instantiates* a copy of the instrument for
each note. Notes are mixed and output from "synthesizer."

# Synth
A Synth is a class that can "play" notes into a mixer. The Synth
abstraction lets you control and process a stream of "notes". A
Synth can be mono or stereo, and if stereo, notes can be panned.

To play a note, you invoke one of:
```
synth.note(instrument_spec, pitch, velocity, optional dur = nil,
           keyword pan = nil, gain = nil, id = nil,
           [*keyword_parameters*])
```
where `instrument_spec` is a dictionary containing 'instrument', a
symbol naming the function that creates an Instrument. When the
Instrument finishes its sound, it must ensure that the output
terminates. Termination means that a terminated flag is set in the
corresponding Arco Ugen. This flag will cause the Synth output mixer
to remove the instrument from the mix and (normally) free the
Instrument unit generators.  Termination can be achieved by calling
.term() with an optional duration (to delay termination, e.g. allowing
for an envelope to decay). Termination can also happen
"automatically", e.g. if the output is a multiplier and one of the
multiplier inputs such as an envelope terminates.

The dictionary is passed to the function along with other parameters:
```
ins = instrument(instrument_spec, pitch, velocity, parameters)
```
If `dur` is supplied to `note`, then after `dur`, the synth will
call `ins.noteoff()`.

If `dur` is not supplied and `id` is given, then at a later time, you
can call
```
synth.noteoff(id)
```
with the originally supplied `id` (a symbol or integer) to end the note.

If `id` is not supplied but `pitch` is an integer, then `pitch` is
used for `id`.

If neither `dur` nor `id` is supplied and `pitch` is float, a warning
is printed and no note is created.

`velocity` is a number from 1 to 127 as in MIDI.

`pan` is specified as a number from 0 to 127 for left to right as in 
MIDI, where 64 is center. If `pan` is omitted, the value of 'pan' from 
`instrument_spec` is used, and otherwise 64. If synth has one channel, 
`pan` is ignored. It is assumed that every instrument returns a mono 
sound and stereo Synths apply panning. `pan` should be ignored by the
instrument itself.

`gain` is specified as a linear scale factor applied to the instrument 
output. If `gain` is omitted, the value of 'pan' from
`instrument_spec` is used, and otherwise 1.0. `gain` should be ignored
by the instrument itself.

*keyword_parameters* are optional keyword parameters. The formal
parameter is declared as a *dictionary* and is passed as `parameters`
to the instrument.

The instrument extracts parameters it needs from both `parameters`
and `instrument_spec`. E.g. to obtain a value for `modulation` with
a default value of zero, the instrument should use:
```
    var m = parameters.get('modulation')
    if not m:
        m = instrument_spec.get('modulation', 0)
    // this can be simplified to:
    var m = parameters.get('modulation') or
            instrument_spec.get('modulation', 0)
```

By convention:

 - `modulation` is a number from 0 to 127 as in MIDI. The
   value is interpreted by the instrument (but can be ignored).

 - `release` is a release time in seconds controlling the time
   it takes from `noteoff` until the sound is no longer heard.
   The shape and perception of the release are instrument-dependent.
   
 - other "universal" parameters will be defined in the future

A call to `synth.note()` returns the instrument so that other
parameters can be controlled. There is no fixed interface shared by
all instruments.

The `instrument_spec` is a mechanism to reduce the number of
parameters one needs to pass to `synth.note()` to make a sound. It
also captures the notion of a "patch" in that the same general
instrument design can be customized through many parameters, and
the parameters can be stored in dictionaries loaded into global
variables. For example, you can have a generic FM instrument and
many specific parameter sets, allowing you so write, simply, 
```
synth.note(FM_HORN_1, C4, LF, 0.7)
```
rather than passing a large number of envelope, frequency, and 
modulation control parameters.

It is recommended that the instrument function taking the 
dictionary should pull out parameters from the dictionary and
pass them explicitly to a class constructor for an Instrument.
That way, the user has two options: using the convenient and
more abstract `synth.note()` interface or creating an instrument
instance directly with all parameters, avoiding the overhead of
unpacking parameters from dictionaries.

The instrument function creates a subclass of Instrument. It
must implement `stop()` (no parameters) which will terminate
the sound.

The Synth offers a way to introduce a note that was not created
internally through a call to `synth.note()`. The method
```
synth.insert(ugen, optional dur, keyword id, pan, gain, 
             [*keyword_parameters*])
```
outputs the ugen using optional pan and gain (defaults 64 and 1.0) and
returns the ugen. If `id` is provided, `synth.noteoff(id)` will work
(provided the ugen implements `stop()`. If both `dur` and `id` are
missing, it is the caller's responsibility to terminate the note.
As with `noteon`, `ugen` must be an Instrument with an envelope.


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

