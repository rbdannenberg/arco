from arco_ugens import *

# blend.srp -- fade between two inputs
#
# Roger B. Dannenberg
# June 2026

BLEND_LINEAR = 0
BLEND_POWER = 1
BLEND_45 = 2

class Blend(Ugen):

    def __init__(self, chans, x1, x2, b, mode, init_b):
        chans = max_chans(chans, x1, x2, b)
        super().__init__(new_ugen_id(), "Blend", chans, A_RATE,
                         "UUUfi", None, None,
                         'x1', x1, "abc",
                         'x2', x2, "abc",
                         'b', b, "abc",
                         'init_b', init_b, "f", 'mode', mode, "i")

    def set_gain(self, gain):
        o2lite.send_cmd("/arco/blend/gain", 0, "if", self.arco_ref(), gain)
        return self

    def set_mode(self, mode):
        o2lite.send_cmd("/arco/blend/mode", 0, "ii", self.arco_ref(), mode)
        return self


def blend(x1, x2, b, chans=None, mode=BLEND_POWER, gain=1, init_b=0.5):
    var ugen = Blend(chans, x1, x2, b, mode, init_b)
    if gain != 1:
        self.set_gain(gain)
    return ugen


class Blendb(Ugen):

    def __init__(self, x1, x2, b, mode=BLEND_LINEAR, chans=None, gain=1):
        chans = max_chans(chans, x1, x2, b)
        super().__init__(new_ugen_id(), "Blendb", chans, B_RATE,
                         "UUUi", None, None,
                         'x1', x1, "bc",
                         'x2', x2, "bc",
                         'b', b, "bc",
                         'mode', mode, "f")

    def set_gain(self, gain):
        o2lite.send_cmd("/arco/blendb/gain", 0, "if", self.arco_ref(), gain)
        return self

    def set_mode(self, mode):
        o2lite.send_cmd("/arco/blendb/mode", 0, "ii", self.arco_ref(), mode)
        return self


def blendb(x1, x2, b, chans=None, mode=BLEND_POWER, gain=1):
    var ugen = Blend(chans, x1, x2, b, mode)
    if gain != 1:
        self.set_gain(gain)
    return ugen
