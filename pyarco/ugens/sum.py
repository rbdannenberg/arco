# sum.py -- sum and sumb unit generators

from arco_ugens import *
from o2litepy import o2lite

def here_is_arco(arco_ref):
    global arco
    # print("sum got arco_ref", arco_ref)
    arco = arco_ref

need_arco_reference(here_is_arco)


class Sum(Ugen):

    def __init__(self, chans, wrap, id_num=None):
        ugens = []
        super().__init__(new_ugen_id(id_num), "Sum", chans,
                         A_RATE, "i", None, None,
                         'wrap', 1 if wrap else 0, "i")
        arco.register_action(self, ACTION_REM, self, "action_rem")

    def action_rem(self, status, uid, parameters):
        # find input with uid if any -- similar to mix but inputs
        # directly map to ugens:
        # print(f"Sum.action_rem: status {status}, uid {uid}, "
        #       f"parameters {parameters}")
        self.inputs.pop(uid, None)

    def ins(self, *ugens):
        for ugen in ugens:
            self.inputs[ugen.arco_ref()] = ugen
            o2lite.send_cmd("/arco/sum/ins", 0, "ii",
                            self.arco_ref(), ugen.arco_ref())
        return self

    def rem(self, *ugens):
        for ugen in ugens:
            name = ugen.arco_ref()
            self.inputs.pop(name, None)
            o2lite.send_cmd("/arco/sum/rem", 0, "ii",
                            self.arco_ref(), ugen.arco_ref())
        return self

    def swap(self, ugen, replacement):
        name = ugen.arco_ref()
        if name in self.inputs:
            # locally, act as if ugen is removed and replacement inserted
            self.inputs.pop(name, None)
            self.inputs[replacement.arco_ref()] = replacement
        o2lite.send_cmd("/arco/sum/swap", 0, "iii", self.arco_ref(),
                        ugen.arco_ref(), replacement.arco_ref())
        return self

    def set_gain(self, gain):
        o2lite.send_cmd("/arco/sum/set_gain", 0, "if", self.arco_ref(), gain)
        return self


def sum(chans=1, wrap=True):
    return Sum(chans, wrap)


class Sumb(Ugen):

    def __init__(self, chans, wrap, id_num=None):
        super().__init__(self, new_ugen_id(id_num), "Sumb", chans,
                          B_RATE, "i", None, None,
                         'wrap', 1 if wrap else 0, "i")
        arco.register_action(self, ACTION_REM, self, "action_rem")

    def action_rem(self, status, uid, parameters):
        # find input with uid if any -- similar to mix but inputs
        # directly map to ugens:
        # print(f"Sumb.action_rem: status {status}, uid {uid}, "
        #       f"parameters {parameters}")
        self.inputs.pop(uid, None)

    def ins(self, *ugens):
        for ugen in ugens:
            self.inputs[ugen.arco_ref()] = ugen
            o2lite.send_cmd("/arco/sumb/ins", 0, "ii",
                            self.arco_ref(), ugen.arco_ref())
        return self

    def rem(self, *ugens):
        for ugen in ugens:
            name = ugen.arco_ref()
            self.inputs.pop(name, None)
            o2lite.send_cmd("/arco/sumb/rem", 0, "ii",
                            self.arco_ref(), ugen.arco_ref())
        return self

    def swap(self, ugen, replacement):
        name = ugen.arco_ref()
        if name in self.inputs:
            # locally, act as if ugen is removed and replacement inserted
            self.inputs.pop(name, None)
            self.inputs[replacement.arco_ref()] = replacement
        o2lite.send_cmd("/arco/sumb/swap", 0, "iii", self.arco_ref(),
                        ugen.arco_ref(), replacement.arco_ref())
        return self

def sumb(chans=1, wrap=True):
    Sumb(chans, wrap)

