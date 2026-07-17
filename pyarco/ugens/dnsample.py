from arco_ugens import *

# 
# dnsampleb.py -- dnsampleb unit generator

class Dnsampleb(Ugen):

    def __init__(self, chans, input, mode):
        super().__init__(new_ugen_id(), "Dnsampleb", chans, B_RATE,
                         "Ui", None, None,
                         'input', input, "a",
                         'mode', mode, "i")

    def set_cutoff(self, hz):
        o2lite.send_cmd("/arco/dnsampleb/cutoff", 0, "if",
                        self.arco_ref(), hz)
        return self

    def set_mode(self, mode):  # refer to the constants, like 'DNSAMPLE_BASIC'
        o2lite.send_cmd("/arco/dnsampleb/mode", 0, "ii",
                        self.arco_ref(), mode)
        return self


def dnsample(input, mode, chans=1):
    Dnsample(chans, input, mode)

DNSAMPLE_BASIC = 0
DNSAMPLE_AVG = 1
DNSAMPLE_PEAK = 2
DNSAMPLE_RMS = 3
DNSAMPLE_POWER = 4
DNSAMPLE_LOWPASS500 = 5
DNSAMPLE_LOWPASS100 = 6
