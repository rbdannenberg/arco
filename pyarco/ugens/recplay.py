from arco_ugens import *

# recplay.py -- record and play audio in memory


def recplay(input, chans=1, gain=1, fade_time=0.1, loop=False):
    return Recplay(chans, input, gain, fade_time, loop)


class Recplay(Ugen):

    def __init__(self, chans, input, gain, fade_time, loop):
        super().__init__(new_ugen_id(), "Recplay", chans, A_RATE,
                         "UUfB", None, None,
                         'input', input, "a",
                         'gain', gain, "bc",
                         'fade_time', fade_time, "f",
                         'loop', loop, "B")

    def record(self, record_flag):
        o2lite.send_cmd("/arco/recplay/rec", 0, "iB", self.arco_ref(), record_flag)
        return self

    def start(self, start_time):
        o2lite.send_cmd("/arco/recplay/start", 0, "id", self.arco_ref(), start_time)
        return self

    def stop(self):
        o2lite.send_cmd("/arco/recplay/stop", 0, "i", self.arco_ref())
        return self

    def set_speed(self, x):
        o2lite.send_cmd("/arco/recplay/speed", 0, "if", self.arco_ref(), x)
        return self

    def borrow(self, u):
        o2lite.send_cmd("/arco/recplay/borrow", 0, "ii", self.arco_ref(), u.id)
        return self
