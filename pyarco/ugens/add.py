from arco_ugens import *

class Add(Ugen):

    def __init__(self, chans=1, wrap=True):
        super().__init__(new_ugen_id(), "Add", chans, A_RATE, "i",
                         None, None, 'wrap', 1 if wrap else 0)

    def ins(self, *ugens):
        for ugen in ugens:
            o2lite.send_cmd("/arco/add/ins", 0, "ii", self.arco_ref(), ugen.id)
        return self

    def rem(self, ugen):
        o2lite.send_cmd("/arco/add/rem", 0, "ii", self.arco_ref(), ugen.id)
        return self

    def swap(self, ugen, replacement):
        o2lite.send_cmd("/arco/add/swap", 0, "iii", self.arco_ref(), ugen.id,
                        replacement.id)
        return self


def add(chans = 1, wrap = true):
    return Add(chans, wrap)


class Addb(Ugen):

    def __init__(self, chans=1, wrap=True):
        super().__init__(new_ugen_id(), "Addb", chans, B_RATE, "i",
                         None, None, 'wrap', 1 if wrap else 0)

    def ins(self, ugen):
        o2lite.send_cmd("/arco/addb/ins", 0, "ii", self.arco_ref(), ugen.id)
        return self

    def rem(self, ugen):
        o2lite.send_cmd("/arco/addb/rem", 0, "ii", self.arco_ref(), ugen.id)
        return self

    def swap(self, ugen, replacement):
        o2lite.send_cmd("/arco/addb/swap", 0, "iii", self.arco_ref(), ugen.id,
                        replacement.id)
        return self


def addb(chans = 1, wrap = true):
    return Addb(chans, wrap)


