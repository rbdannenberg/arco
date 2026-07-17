from arco_ugens import *

# dualslewb.py -- dualslewb unit generator

class Dualslewb(Ugen):

    def __init__(self, chans, input, chans, attack, release,
                 current, attack_linear, release_linear):
        super().__init__(new_ugen_id(), "Dualslewb", chans, B_RATE,
                         "Ufffii", None, None,
                         'input', input, "bc",
                         'attack', attack, "f",
                         'release', release, "f",
                         'current', current, "f",
                         'attack_linear', attack_linear, "f",
                         'release_linear', release_linear, "f")

    def set_current(self, current):
        o2lite.send_cmd("/arco/dualslewb/current", 0, "if",
                        self.arco_ref(), current)
        return self

    def set_attack(self, attack, attack_linear=True):
        o2lite.send_cmd("/arco/dualslewb/attack", 0, "ifi",
                        self.arco_ref(), attack, 1 if attack_linear else 0)
        return self

    def set_release(self, release, release_linear=True):
        o2lite.send_cmd("/arco/dualslewb/release", 0, "ifi",
                        self.arco_ref(), release, 1 if release_linear else 0)
        return self


def dualslewb(input, attack=0.02, release=0.02, current=0,
              chans=1, attack_linear=True, release_linear=True):
    return Dualslewb(chans, input, attack, release, current,
                     1 if attack_linear else 0, 1 if release_linear else 0)
