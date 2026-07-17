from arco_ugens import *

# spectralrolloff.py -- audio analysis

class SpectralCentroid(Ugen):

    def __init__(self, input, reply_addr):
        super().__init__(new_ugen_id(), "SpectralCentroid", 0, NO_RATE,
                         "isf", None, True,
                         'input', input, "a", 'reply_addr', reply_addr, "s")

    def start(self, reply_addr):
        o2lite.send_cmd("/arco/spectralcentroid/start", 0, "is",
                        self.arco_ref(), reply_addr)
        return self


def spectralcentroid(input, reply_addr):
    return SpectralCentroid(input, reply_addr):
