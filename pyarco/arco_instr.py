import math
import random

from arco_ugens import Ugen, Const, Param_descr, ACTION_END_OR_TERM
from ugens.smoothb import Smoothb
from ugens.sum import Sum, Sumb
from ugens.route import Route
from ugens.mix import Mix

SIGNAL = 'signal'
GAIN = 'gain'
BOTH = 'both'
MUTE = 'mute'
FINISH = 'finish'

# ======================== Instrument parameter framework =====================

# Stack used during instrument construction (between instr_begin and
# Instrument.__init__) to collect parameter descriptors.
_instr_stack = []


def instr_begin():
    """Call at the start of an Instrument subclass __init__."""
    _instr_stack.append({})


def _add_param_descr_to_context(pd, name):
    context = _instr_stack[-1]
    if name in context:
        print("WARNING: Parameter", name, "is already specified. Ignored.")
    else:
        context[name] = pd
    return pd.value


def param(name,
          initial_value,
          op=None,
          low=None,
          high=None,
          smooth=False,
          chans=1):
    """Declare a setable parameter backed by a Const or Smoothb UGen.

    Returns the UGen so it can be wired into the instrument graph.
    """
    if isinstance(smooth,
                  (int, float)) and smooth and not isinstance(smooth, bool):
        ugen = Smoothb(initial_value, cutoff=smooth)
    elif smooth:
        ugen = Smoothb(initial_value)
    else:
        ugen = Const(initial_value)
    pd = Param_descr(ugen, initial_value, op, low, high)
    _add_param_descr_to_context(pd, name)
    return ugen


def param_map(name, initial_value, subinstr, subinstr_name=None):
    """Declare a parameter that delegates to a sub-instrument's parameter."""
    if subinstr_name is None:
        subinstr_name = name
    pd = Param_descr(subinstr, initial_value, subinstr_name, None, None)
    _add_param_descr_to_context(pd, name)


def param_method(name, initial_value, method, op=None, low=None, high=None):
    """Declare a parameter handled by calling a method on the instrument."""
    pd = Param_descr(method, initial_value, op, low, high)
    _add_param_descr_to_context(pd, name)
    return initial_value


class Instrument(Ugen):
    """An Instrument wraps a UGen graph with named, setable parameters.

    Subclasses call instr_begin() at the top of __init__, build the graph,
    then call super().__init__(name, output_ugen) at the end.
    """

    def __init__(self, name, output_ugen, synth=None):
        self.synth = synth
        self.mixer_id = None
        self.user_id = None
        self.pitch = None
        self.vel = None
        self.gain = None

        if synth is not None:
            self.mixer_id = synth.get_mixer_id()

        self.output = output_ugen
        # Inherit the output ugen's id so that wiring this Instrument
        # into the graph is the same as wiring its output.
        super().__init__(output_ugen.id_num,
                         name,
                         output_ugen.chans,
                         output_ugen.rate,
                         "",
                         no_msg=True)

        if len(_instr_stack) == 0:
            raise RuntimeError(
                "instr_stack is empty. Did you forget instr_begin()?")
        self.parameter_bindings = _instr_stack.pop()


    def __del__(self):
        """when instrument is deleted, we do NOT free self.arco_ref()
        because self.output has the same arco_ref() and it will be
        freed as well (in due time)
        """
        pass
        # print("__del__ freeing Instrument", self, "output", self.output,
        #       "arco_ref", self.output.arco_ref())


    def get(self, input_name):
        return self.parameter_bindings.get(input_name)

    def set(self, input_name, value, reuse=False):
        pd = self.parameter_bindings.get(input_name)
        if pd is not None:
            pd.set(self, value, reuse)

    def finish(self, status, finisher, parameters):
        if self.synth and (status & ACTION_END_OR_TERM) > 0:
            self.synth.is_finished(self)




