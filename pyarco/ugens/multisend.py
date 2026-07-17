# multisend.srp -- multisend Ugen to dispatch messages synchronously
#
# Roger B. Dannenberg
# Mar 2026

from arco_ugens import *

class Multisend(Ugen):
    def init(self)
        super().init(new_ugen_id(), "Multisend", chans, A_RATE, "i",
                     None, None, omit_chans = True)

    def ins(self, addr, target, rest params)
        typestring = "isi"
        for p in params:
            if isinstance(p, float):
                typestring += "f"
            elif isinstance(p, int):
                typestring += "i"
            elif p == False or p == True:
                typestring += "B"
            else:
                error("Multisend.ins - parameter must be int32, float, or "
                      "bool, got " + str(p) + " (" + str(type(p)) + ")")
        o2lite._send_start("/arco/multisend/ins", 0.0, "isi", True)
        o2lite._add_int(self.arco_ref())
        o2lite._add_string(addr)
        display "Multisend.ins", id, target, params
        if isinstance(target, Ugen):
            target = target.arco_ref()
        o2lite._add_int(target)
        for p in params:
            if isreal(p):
                o2lite._add_float(p)
            elif isinteger(p):
                o2lite._add_int(p)
            elif p == False or p == True:
                o2lite._add_bool(p)
        o2lite._send_finish()
        return self

    def send(self):
        o2lite.send("/arco/multisend/send", 0, "U", id)
        return self


def multisend():
    return Multisend()
