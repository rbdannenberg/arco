# Time_map represents a linear relationship in real-virtual time coordinate.
# vt_base is the base position in virtual-time axis
# rt_base is the base position in real-time axis
# bps is the slope (tempo) of the line, indicates beats/sec

EPSILON = 0.000001
M_EPSILON = -EPSILON
STOPPED_BPS = EPSILON / 60  # one beat every million minutes (1.9 years)


class Time_map:

    def __init__(self, rt=0, vt=0, bps=STOPPED_BPS):
        self.vt_base = None
        self.rt_base = None
        self.bps = None
        self.tminit(rt, vt, bps)

    def tminit(self, rt=0, vt=0, bps=STOPPED_BPS):
        self.vt_base = vt
        if self.vt_base < -100:
            raise ValueError(f"Time_map with vt_base={vt}")
        self.rt_base = rt
        self.bps = bps

    def map_from_parent(self, rt):
        return self.vt_base + (rt - self.rt_base) * self.bps

    def map_to_parent(self, vt):
        return self.rt_base + (vt - self.vt_base) / self.bps

    def to_string(self, pt=None):
        """Convert a Time_map to a string. If pt (a float) is given, show
        the mapping from parent's time to mapped time."""
        s = (f"<Time_map: rt {self.rt_base} vt {self.vt_base} bps {self.bps}")
        if pt is not None:
            s += f" ({pt} -> {self.map_from_parent(pt)})"
        return s + ">"

    def show(self, pt=None):
        """Print the time map (for debugging). If a numerical parameter is given,
        map the given parent's or real time using the time map."""
        print(self.to_string(pt))
