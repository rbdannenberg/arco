# thru.py -- audio pass-through

from arco_ugens import *

class Thru(Ugen):

    def __init__(self, chans, input, id_num=None):
        super().__init__(new_ugen_id(id_num),
                         "Thru", chans, A_RATE,
                         "U", None, None, 'input', input, "a")

    def set_alternate(self, alt):
        o2lite.send_cmd(new_ugen_id(), "/arco/thru/alt", 0, "ii",
                        self.arco_ref(), alt.arco_id())
        return self


def thru(input, chans=1):
    Thru(chans, input)

def fanout(input, chans=1):
    """When thru is used for fanout, the preferred constructor is "fanout",
       which requires the number of channels you are expanding to. You should
       only use this if input is mono.
    """
    Thru(input, chans)
