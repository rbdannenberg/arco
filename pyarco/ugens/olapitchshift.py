from arco_ugens import *

# olapitchshift.py -- overlap add pitch shift unit generator

class Ola_pitch_shift(Ugen):

    def __init__(self, chans, input, ratio, xfade, windur):
        super().__init__(new_ugen_id(), "Olaps", chans, A_RATE,
                         "Ufff", None, None,
                         'input', input, "a", 'ratio', ratio, "f",
                         'xfade', xfade, "f", 'windur', windur, "f")

    def set_ratio(self, value):
        o2lite.send_cmd("/arco/olaps/ratio", 0, "if", self.arco_ref(), value)
        return self

    def set_xfade(self, xfade):
        o2lite.send_cmd("/arco/olaps/xfade", 0, "if", self.arco_ref(), xfade)
        return self

    def set_windur(self, windur):
        o2lite.send_cmd("/arco/olaps/windur", 0, "if", self.arco_ref(), windur)
        return self


def olapitchshift(input, ratio, xfade, windur, chans=1):
    Olapitchshift(chans, input, ratio, xfade, windur)
