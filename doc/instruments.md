# Arco Instruments and Effects

**Roger B. Dannenberg**

## Introduction
Beyong unit generators (see [ugens.md](ugens.md)), there are instruments and effects constructed from unit generators and available as Serpent files, described here.

## Reverb
A simple reverberation effect, based on Schroeder reverb design, is in `reverb.srp`. To create:
```
rvb = reverb(inp, rt60)
```
where `inp` is the reverb input signal (a-rate) and `rt60` is the reverb time (a float, not a signal). Methods include:
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

