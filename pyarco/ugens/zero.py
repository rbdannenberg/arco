# zero.srp -- audio zero
#
# Roger B. Dannenberg
# Jul 2026, ported from zero.srp

from arco_ugens import *

class Zero(Ugen):

    def __init__(self, id_num=None):
        super().__init__(new_ugen_id(id_num), "Zero", 1, A_RATE, "",
                         omit_chans=True)


class Zerob(Ugen):

    def __init__(self, id_num=None):
        super().__init__(new_ugen_id(id_num), "Zerob", 1, B_RATE, "",
                         omit_chans=True)

def zero():
    return arco.zero

def zerob():
    return arco.zerob
