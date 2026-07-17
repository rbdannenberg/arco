from arco_ugens import *

# spectralrolloff.py -- audio analysis

class SpectralRolloff(Ugen):

    def __init__(self, input, reply_addr, threshold):
        super().__init__(new_ugen_id(), "SpectralRolloff", 0, NO_RATE,
                         "isf", None, True,
                         'input', input, "a", 'reply_addr', reply_addr, "s",
                         'threshold', threshold, "f")

    def start(self, reply_addr):
        o2lite.send_cmd("/arco/spectralrolloff/start", 0, "is",
                        self.arco_ref(), reply_addr)
        return self


def spectralrolloff(input, reply_addr, threshold):
    return SpectralRolloff(input, reply_addr, threshold):
