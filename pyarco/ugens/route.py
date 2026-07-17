# route.srp -- route input channels to output channels

from arco_ugens import *

class Route(Ugen):

    def __init__(self, chans):
        super().__init__(new_ugen_id(), "Route", chans, A_RATE, "",
                         None, None)

        self._send_ins_rem(input, routes, "/arco/route/ins")
        return self

    def rem(self, input, *routes):
        self._send_ins_rem(input, routes, "/arco/route/rem")
        return self

    def reminput(self, input):
        o2lite.send_cmd("/arco/route/reminput", 0, "ii",
                        self.arco_rec(), input.arco_ref())
        return self

    def _send_ins_rem(self, input, routes, address):
        o2lite.send_cmd(address, 0, "ii" + ("i" * len(params)),
                        self.arco_ref(), input.arco_ref(), *routes)


def route(chans):
    return Route(chans)
