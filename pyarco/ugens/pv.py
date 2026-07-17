from arco_ugens import *

# pv.py -- overlap add pitch shift unit generator


class Pv(Ugen):

    def __init__(self, chans, input, ratio, fftsize, hopsize, points, mode):
        super().__init__(new_ugen_id(), "Pv", chans, A_RATE,
                         "Ufiiii", None, None,
                         'input', input, "a",     'ratio', ratio, "f",
                         'fftsize', fftsize, "f", 'hopsize', hopsize, "f",
                         'points', points, "f", 'mode', mode, "f")

    def set_ratio(self, value):
        o2lite.send_cmd("/arco/pv/ratio", 0, "if", self.arco_ref(), value)
        return self

    def set_stretch(self, value):
        o2lite.send_cmd("/arco/pv/stretch", 0, "if", self.arco_ref(), value)
        return self

def pv(input, ratio, fftsize, hopsize, points, mode, chans=1):
    Pv(chans, input, ratio, fftsize, hopsize, points, mode)
