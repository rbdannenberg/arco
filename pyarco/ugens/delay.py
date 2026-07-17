from arco_ugens import *

# delay.py -- feedback delay unit generator

class Delay(Ugen):

    def __init__(self, chans, input, dur, fb, maxdur):
        chans = max_chans(chans, input, dur, fb)
        super().__init__(new_ugen_id(), "Delay", chans, A_RATE,
                         "UUUf", None, None, 'input', input, "a",
                         'dur', dur, "abc", 'fb', fb, "abc",
                         'maxdur', maxdur, "f")

def delay(input, dur, fb, maxdur, chans=None):
    Delay(chans, input, dur, fb, maxdur)
