from arco_ugens import *

# vu.py -- record and play audio in memory

def vu(reply_addr, period):
    Vu(reply_addr, period)


class Vu(Ugen):

    def __init__(self, reply_addr, period):
        super().__init__(new_ugen_id(), "Vu", 0, '', "sf", None, None,
                         'reply_addr', reply_addr, "s",
                         'period', period, "f")

    def start(self, reply_addr, period):
        o2lite.send_cmd("/arco/vu/start", 0, "isf", self.arco_ref(),
                        reply_addr, period)
        return self

    def set(self, input_name, value):
        self.inputs['input'] = value
        o2lite.send_cmd("/arco/vu/repl_input", 0, "ii",
                        self.arco_ref(), value.arco_ref())
        return self
