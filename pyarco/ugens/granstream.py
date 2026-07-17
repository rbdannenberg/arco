from arco_ugens import *

# granstream.py -- granular synthesis from input stream unit generator

class Granstream(Ugen):

    def __init__(self, chans, input, polyphony, dur, enable):
        super().__init__(new_ugen_id(), "Granstream", chans, A_RATE,
                         "UifB", None, None,
                         'input', input, "a", 'polyphony', polyphony, "i",
                         'dur', dur, "f",     'enable', enable, "B")

    def set_gain(self, gain):
        o2lite.send_cmd("/arco/granstream/gain", 0, "if", self.arco_ref(), gain)
        return self

    def set_polyphony(self, p):
        o2lite.send_cmd("/arco/granstream/polyphony", 0, "if",
                        self.arco_ref(), p)
        return self

    def set_ratio(self, low, high):
        o2lite.send_cmd("/arco/granstream/ratio", 0, "iff",
                        self.arco_ref(), low, high)
        return self

    def set_graindur(self, lowdur, highdur):
        o2lite.send_cmd("/arco/granstream/graindur", 0, "iff",
                        self.arco_ref(), lowdur, highdur)
        return self

    def set_density(self, density):
        o2lite.send_cmd("/arco/granstream/density", 0, "if",
                        self.arco_ref(), density)
        return self

    def set_env(self, attack, release):
        o2lite.send_cmd("/arco/granstream/env", 0, "iff",
                        self.arco_ref(), attack, release)
        return self

    def set_enable(self, enable):
        o2lite.send_cmd("/arco/granstream/enable", 0, "iB",
                        self.arco_ref(), enable)
        return self

    def set_dur(self, dur):
        o2lite.send_cmd("/arco/granstream/dur", 0, "if", self.arco_ref(), dur)
        return self

    def set_delay(self, d):
        o2lite.send_cmd("/arco/granstream/delay", 0, "if", self.arco_ref(), d)
        return self

    def set_feedback(self, fb):
        o2lite.send_cmd("/arco/granstream/feedback", 0, "if",
                        self.arco_ref(), fb)
        return self


def granstream(input, polyphony, dur, enable, chans=1):
    Granstream(chans, input, polyphony, dur, enable)
