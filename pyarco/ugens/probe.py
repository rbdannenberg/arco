from arco_ugens import *

# probe.py -- stream audio from file

def probe(input, reply_addr)
    return Probe(input, reply_addr)



class Probe(Ugen):

    def __init__(self, input, reply_addr):
        self.running = False
        super().__init__(new_ugen_id(), "Probe", 0, NO_RATE, "Us",
                         None, True,
                         'input', input, "abc",
                         'reply_addr', reply_addr, "s")

    def probe(self, period, frames, chan, nchans, stride):
        o2lite.send_cmd("/arco/probe/probe", 0, "ifiiii",
                        self.arco_ref(), period, frames, chan, nchans, stride)
        if not self.running:
            self.run()  # suppress warning if we're already in run set
            self.running = True
        return self

    def thresh(self, threshold, direction, max_wait):
        o2lite.send_cmd("/arco/probe/thresh", 0, "ifif",
                        self.arco_ref(), threshold, direction, max_wait)
        return self

    def stop(self):
        o2lite.send_cmd("/arco/probe/stop", 0, "i", self.arco_ref())
        if self.running:
            self.unrun()
            self.running = False
        return self
