from arco_ugens import *

# trig.py -- sound event detection

class Trig(Ugen):

    def __init__(self, input, reply_addr, window, threshold, pause):
        super().__init__(new_ugen_id(), "Trig", 0, A_RATE, "Usiff",
                         None, True,
                         'input', input, "a",   'reply_addr', reply_addr, "s",
                         'window', window, "f", 'threshold', threshold, "f",
                         'pause', pause, "f")

    def set_window(self, window):
        o2lite.send_cmd("/arco/trig/window", 0, "ii", self.arco_ref(), window)
        return self

    def set_threshold(self, threshold):
        o2lite.send_cmd("/arco/trig/threshold", 0, "if",
                        self.arco_ref(), threshold)
        return self

    def set_pause(self, pause):
        o2lite.send_cmd("/arco/trig/pause", 0, "if", self.arco_ref(), pause)
        return self

    def onoff(self, reply_addr, threshold, runlen):
        o2lite.send_cmd("/arco/trig/onoff", 0, "isff",
                        self.arco_ref(), reply_addr, threshold, runlen)
        return self


def trig(input, reply_addr, window, threshold, pause):
    Trig(input, reply_addr, window, threshold, pause)
