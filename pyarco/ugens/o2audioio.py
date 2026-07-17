# o2audioio.py -- overlap add pitch shift unit generator
#
# Roger B. Dannenberg
# July 2026 (from o2audioio.srp)

from arco_ugens import *


class O2audioio(Ugen):
    def init(self, input, destaddr, destchans, recvchans, buffsize,
             sampletype, msgsize):
        super.init(new_ugen_id(), "O2audioio", recvchans, 'a', "Usiiii",
                   None, None, 'input', input, "a", 'destaddr', destaddr, "f",
                   'destchans', destchans, "f", 'buffsize', buffsize, "f",
                   'sampletype', sampletype, "f", 'msgsize', msgsize, "f")
    
    def enable(self, value):
        if value == true:
            value = 1
        elif value == false:
            value = 0
        o2_send_cmd("/arco/o2aud/enab", 0, "Ui", id, value)
        return self


def o2audioio(input, destaddr, destchans, recvchans, buffsize,
              sampletype, msgsize)
    return O2audioio(input, destaddr, destchans, recvchans, buffsize,
                     sampletype, msgsize)
