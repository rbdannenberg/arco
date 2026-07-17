from arco_ugens import *

class Stdistr(Ugen):

    def __init__(self, n, width):
        super().__init__(new_ugen_id(), "Stdistr", 2, A_RATE, "if", None,
                         True, 'n', n, 'width', width)

    def set_gain(self, gain):
        o2lite.send_cmd("/arco/stdistr/gain", 0, "if", self.arco_ref(), gain)
        return self

    def set_width(self, width):
        o2lite.send_cmd("/arco/stdistr/width", 0, "if", self.arco_ref(), width)
        return self

    def ins(self, index, ugen):
        o2lite.send_cmd("/arco/stdistr/ins", 0, "iii", self.arco_ref(), index, ugen.id)
        return self

    def rem(self, index):
        o2lite.send_cmd("/arco/stdistr/rem", 0, "ii", self.arco_ref(), index)
        return self


def stdistr(n, width):
    return Stdistr(n, width)
