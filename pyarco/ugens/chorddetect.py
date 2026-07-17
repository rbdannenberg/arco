from arco_ugens import *

# chorddetect.py -- chord detection based on chroma


class Chorddetect(Ugen):

    def __init__(self, input, reply_addr):
        super().__init__(new_ugen_id(), "Chorddetect", 0, NO_RATE,
                         "Us", None, True,
                         'input', input, "a", 'reply_addr', reply_addr, "s")

    def start(self, reply_addr):
        o2lite.send_cmd("/arco/chorddetect/start", 0, "is", self.arco_ref(),
                        reply_addr)
        return self


def chorddetect(input, reply_addr):
    return Chorddetect(input, reply_addr)
