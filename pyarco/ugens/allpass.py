# allpass.py -- feedback all-pass filter unit generator
#
# Roger B. Dannenberg
# July 2026 (from allpass.srp)

from arco_ugens import *

class Allpass (Ugen):
    def init(self, chans, input, dur, fb, maxdur):
        super().init(new_ugen_id(), "Allpass", chans, 'a', "UUUf", None, None,
                     'input', input, "a", 'dur', dur, "abc", 'fb', fb, "abc",
                     'maxdur', maxdur, "f")


def allpass(input, dur, fb, maxdur, chans = 1):
    return Allpass(chans, input, dur, fb, maxdur)  # allpass as a function


