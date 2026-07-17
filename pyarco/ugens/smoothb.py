# smoothb.py -- class for Smoothb unit generator

from arco_ugens import *

class Smoothb(Const_like):
    """ 
    A 1st order low-pass filter for smoothing control signals

    Parameters
    ----------
    x
        is initial value or array of initial values
        if len(x) > chans (consider len(x) = 1 if isnumber(x)),
        then extra values are ignored. if len(x) < chans, then
        if isnumber(x), *all* channels are set to x, and otherwise
        channels from len(x) are initialized to zero
    cutoff
        is the lowpass cutoff frequency for smoothing
    chans
        defaults to size of x
    """

    def __init__(self, x, cutoff=10, chans=None):
        chans = max_chans(chans, x)
        super().__init__(new_ugen_id(), "Smoothb", x, chans,
                         "/arco/smoothb/newn")

    def set(self, x):
        self.send_floats(x, "/arco/smoothb/setn")
        return self

    def set_chan(self, chan, x):
        o2lite.send_cmd("/arco/smoothb/set", 0, "iif", self.arco_ref(), chan, x)
        return self

    def set_cutoff(self, cutoff):
        o2lite.send_cmd("/arco/smoothb/cutoff", 0, "if", self.arco_ref(), cutoff)
        return self


def smoothb(x, cutoff=10, chans=None):
    return Smoothb(x, cutoff, chans)
