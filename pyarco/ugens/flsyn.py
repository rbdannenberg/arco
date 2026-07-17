from arco_ugens import *

class Flsyn(Ugen):

    def __init__(self, path):
        # already hard-coded as 2 channels in flsyn.h
        super().__init__(new_ugen_id(), "Flsyn", None, A_RATE, "s", None,
                         True, 'path', path)

    def alloff(self, chan):
        o2lite.send_cmd("/arco/flsyn/off", 0, "ii", self.arco_ref(), chan)
        return self

    def control_change(self, chan, num, val):
        o2lite.send_cmd("/arco/flsyn/cc", 0, "iiii", self.arco_ref(), chan, num, val)
        return self

    def channel_pressure(self, chan, val):
        o2lite.send_cmd("/arco/flsyn/cp", 0, "iii", self.arco_ref(), chan, val)
        return self

    def key_pressure(self, chan, key, val):
        o2lite.send_cmd("/arco/flsyn/kp", 0, "iiii", self.arco_ref(), chan, key, val)
        return self

    def noteoff(self, chan, key):
        o2lite.send_cmd("/arco/flsyn/noteoff", 0, "iii", self.arco_ref(), chan, key)
        return self

    def noteon(self, chan, key, vel):
        o2lite.send_cmd("/arco/flsyn/noteon", 0, "iiii", self.arco_ref(), chan, key,
                        vel)
        return self

    def pitch_bend(self, chan, bend):
        o2lite.send_cmd("/arco/flsyn/pbend", 0, "iif", self.arco_ref(), chan, bend)
        return self

    def pitch_sens(self, chan, val):
        o2lite.send_cmd("/arco/flsyn/psens", 0, "iii", self.arco_ref(), chan, val)
        return self

    def program_change(self, chan, program):
        o2lite.send_cmd("/arco/flsyn/prog", 0, "iii", self.arco_ref(), chan, program)
        return self

