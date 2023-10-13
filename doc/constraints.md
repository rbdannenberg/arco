# Constraints

**Roger B. Dannenberg**

## Introduction
Constraints in Arco offer a style of programming where unit generator
parameters and other values can be constrained to follow GUI controls
and other input data.

Consider the problem of sending microphone input directly to the audio
output, using a slider to control gain.

In "normal" event-driven object-oriented programming, we would use
callbacks from GUI controls to react to changes in the slider. In
wxSerpent, we use something like `.add_target_method(obj, 'gain_change')`
to get a callback to method `gain_change` in some object, and then
we define `gain_change` to update the gain in a mixer or multiplier.

In contrast, in a visual programming language like Max or Pd, we would
probably just connect the slider to the multiply without all the "glue
code" to receive and act on slider events.

A corresponding approach is provided by Arco *constraints*, which allow
you to "declare" a connection from GUI control (slider) to unit generator.
Of course, it is still text-based programming, but it seems to provide
a principled way to extend the data-flow properties of signal processing
at audio- and control-rates inside the Arco server to the realm of
"constant-rate" parameter computation happening in the control program.

## The Constraint Class
A Constraint is an abstract superclass that implements one-way constraints
from GUI controls to Arco `const` and `smoothb` unit generators. Specifically,
a constraint computes a single value from a set of input parameters, which
are represented as instance variables. The output is recomputed (normally)
when *any* input parameter changes, so that the output is always a function
of the current value of inputs.

Any change to an instance variable is made by calling `set(name, value)`
where `name` is the symbol representing the instance variable (e.g. `'x'`)
and `value` is the new value. This call triggers the recomputation
of the constraint and propagates the value.

### Subclass responsibilities
For each constraint formula, a subclass of `Constraint` must be defined.
To make constraints general, we allow any number of input parameters, which
must be declared as instance variables. The constraint function must be
defined in the method `compute()` which has no explicit parameters, but
which ordinarily is a function of various instance variables. The
`compute()` method returns a computed value that is considered to be the
output value of the constraint.

In some cases, you may not want to recompute the constraint when a particular
instance variable changes. For example, you may know that instance variables
are set in sequence and it is better to wait for the last variable in the
sequence to be set. Or you might want to drive recomputation by a single
variable update that occurs periodically.  To control constraint computation
and propagation, you can call `passive(name)` with the name of an instance
variable. Then, changes to the variable will not trigger a recomputation.

Details: Note that simply assigning the instance variable (e.g. `obj.x = 5`)
will *not* cause recomputation in any case. You must use `obj.set('x', 5)`.
Also, computation will only occur if the value actually changes so
periodically calaling `obj.set('x', t)` will not cause the constraint to be
updated, but this will: `obj.set('x', not obj.x)` since it is guaranteed to
change `x`.

## Setting dependencies
To "connect" a constraint to a GUI control, use the `connect(control, name)`
method. `control` is a GUI control created by `Spinctrl`, `Slider`,
`Labeled_slider`, or `Checkbox` (a Checkbox is considered to output 0 or 1
even though it's "native" output is Boolean) *or* `control` can be a subclass
of another `Constraint`. (Do not create circular constraint dependencies!)

The `name` parameter is the symbol representing the instance variable to
be set to the value of the control. This parameter is optional and defaults
to `'x'`.

## Creating dependents
If you want to use the *output* of the constraint in an audio computation,
you can create a `const` or `smoothb` unit generator by calling either
`constraint.get_const()` or `constraint.get_smoothb(cutoff)`, where
'constraint` is the constraint (object) and `cutoff` is the cutoff
frequency. `cutoff` is optional and defaults to 10 Hz. These both provide
block-rate signals, which are normally interpolated, so even `const` gets
some smoothing as a by-product of up-sampling by linear interpolation
when it is used to control audio gain.

Once you have called `get_const` or `get_smoothb`, a second call will
return the same `const` ugen because ugens in Arco can fan out to any
number of inputs.

You can also "convert" constraint updates to callbacks by calling
`constraint.add_target_method(target, method)` where `target` is an
object and `method` is a method of the target (or if `target` is `nil`,
`method` names a global function). The method or function is called
with two parameters: the constraint (object) and its new value.
You can have any number of target/method pairs in combination with
either one `const` or one `smoothb` (but not both).

## Cleaning Up
When can a constraint be garbage collected? When a constraint is
connected to a control, there is a reference from the control to
the constraint. Freeing the control will free the constraint. But
to remove the constraint while leaving the control intact requires
a call to the `Constraint.finish()` method. This removes the
constraint from the control's target objects and removes the
constraint's reference to a ugen. If the ugen is in use, it will
continue to provide the current constant values.

In short, call a constraint's `finish` method when you no longer
need it.

## Example
Getting back to our initial example, controlling input-to-output gain
with a slider, the code in Serpent looks like the following:

You need the constraint implementation before you can use Constraint:
```
require "constraint"  // you need the constraint implementation
```
Create a slider. Window position is (10, 5), size is (250, 'H') where
H means the default height, and the label width is 80. The range of
the slider is -60 to 20 using the 'db' mapping from slider position to
decibels. The name of the slider is `'monitor_gain_sl'` which will
assign the slider to this global variable for convenience, and `prefs`
is a preferences object that is already created and open. This will
be used to automatically initialize the slider to a stored preference,
using preference name `'monitor_gain_sl'`, and if you save preferences,
the current value of the slider will be saved.
```
Labeled_slider(win, "Monitor Input", 10, 5, 250, 'H',
               80, -60, 20, 0, 'db', 'monitor_gain_sl', prefs = prefs)
```
Next create a constraint. We're going to pan a mono signal to stereo
output, so the constraint is going to output an array of 2 gain values
that depend on the instance variable `'x'`, which already exists in
`Simple_constraint` so we'll subclass that. All we need to define is
the compute method, which converts `x` from dB to linear and calls
`pan_45` to pan to center using the 4.5 dB pan law:
```
class Center_constraint (Simple_constraint):
    def compute(): return pan_45(0.5, db_to_linear(x))
```
We need an instance of `Center_constraint` to manage our output gain.
Then, we connect it to the slider. The `trace` variable is entirely
optional: Setting it will cause constraint evaluations to be printed
so you can see the contraint at work. Normally, set this only for
debugging because it will generate a lot of output.
```
monitor_gain_constraint = Center_constraint()
monitor_gain_constraint.trace = "monitor_gain_constraint"
monitor_gain_constraint.connect(monitor_gain_sl)
```
Finally, we can implement the audio processing. We use `get_const()`
to obtain the pan and gain control. This will make a two-channel
const ugen. When we multiply that by a single channel audio signal,
we get a stereo output that we can send to the audio output. To
make sure we have a single channel audio signal (e.g. we might have
a stereo input with the mono input on the left channel), we'll use
a route ugen to select just the first channel (designated by the
second parameter, 0) regardless of how many input channels there are:
```
monitor_gain = mult(route(input_ugen, 0),
                    monitor_gain_constraint.get_const())
monitor_gain.play()  // connect the mult ugen to audio output
```
If you want to delete this audio process, you should stop playing,
"finish" the constraint, and remove references to the objects. The
const ugen is referenced by both the mult ugen and the constraint,
so when they are both garbage collected, the const will also be
garbage collected:
```
monitor_gain.mute()
monitor_gain_constraint.finish()
monitor_gain = nil
monitor_gain_constraint = nil
```

I'm always thinking about visual vs. textual programminig systems and
their relative merits. For Arco , that's 9 lines of code to send input
to output with gain control, but the Center_constraint class is
reusable, so maybe 7 lines. If you were controlling a lot of different
sources in the same way, you could obviously write a function to
create a slider, build a constraint and route the audio to simplify
things further. As a one-time feature, maybe 9 lines is more work than
the corresponding visual programming construction. With visual
programming, though, adding the panning and channel assignments
creates some extra work, and the ability of textual approaches like
this to encapsulate, parameterize, and reuse little building blocks
is a big advantage of textual programming languages.
