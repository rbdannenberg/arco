from arco_ugens import *

# feedback.py -- mix input with most recent output of some Ugen
#
# Roger B. Dannenberg
# June 2026

class Feedback(Ugen):
    """ This unit generator scales "from" by gain and adds to input to form
    output. The "from" update method is not called until *after* computing
    output, which allows for "from" to depend on our output without causing
    infinite recursion, but at the cost of using "stale" samples from the
    "from" source, which will normally be the previous 32 samples, creating
    feedback with a 32-sample delay.
    """
    def __init__(self, chans, input, from_ugen, gain):
        chans = max_chans(chans, input)
        super().__init__(new_ugen_id(), "Feedback", chans, A_RATE,
                         "U", None, None, 'input', input, "a")

    def fb(from, gain):
        if isinstance(from, Param_descr):
            from = from.get_ugen_value("Feedback", 'from')
        if isinstance(gain, Param_descr):
            gain = gain.get_ugen_value("Feedback", 'gain')
        set('from', from)
        set('gain', gain)


def feedback(input, chans=None):
    Feedback(chans, input)
