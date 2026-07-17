from arco_ugens import *

class Yin(Ugen):

    def __init__(self, input, minstep, maxstep, hopsize, address, chans=1):
        super().__init__(new_ugen_id(), "Yin", chans, A_RATE, "Uiiis",
                         None, None, 'input', input, 'minstep', minstep,
                         'maxstep', maxstep, 'hopsize', hopsize,
                         'address', address)


def yin(input, minstep, maxstep, hopsize, address, optional chans = 1):
    return Yin(chans, input, minstep, maxstep, hopsize, address)  # yin as a function
