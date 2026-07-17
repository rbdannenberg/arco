# envelope.py - the Envelope class inherited by pwl, pwe, pwlb, pweb, etc.

from arco_ugens import *

class Envelope(Ugen):

    def __init__(self, classname, addr, rate, points):
        self.address = addr
        super().__init__(new_ugen_id(), classname, 1, rate, "", None, True)
        self.sample_rate = AR if rate == A_RATE else BR
        self.set_point_array(points)

    def set_point_array(self, points):
        # define the shape of the envelope with an array of floats, 
        # d0 y0 d1 y1 ... where di is duration (secs) and yi is amplitude.
        # If the length is odd, a final yn = 0.0 is appended.
        params = []
        time = 0
        count = 0

        for i in range(0, len(points), 2):
            time += points[i]
            # Avoid accumulated rounding error from converting durations to
            # block counts as follows:
            #   count is the block count where this segment begins
            #   time * BR is the exact fractional block count where it ends
            #   samps is therefore the duration we want with compensation
            #   for any accumulated rounding error.
            samples = max(1, round(time * self.sample_rate - count))
            count += samples
            params.append(float(samples))  # the number of samples to ramp
            if i + 1 < len(points):  # allowed to omit final zero amplitude
                params.append(float(points[i + 1]))  # destination amplitude
        o2lite.send_cmd(self.address + "env", 0, "i" + "f" * len(params),
                        self.arco_ref(), *params)
        return self

    def set_points(self, *points):
        self.set_point_array(points)
        return self

    def start(self):
        o2lite.send_cmd(self.address + "start", 0, "i", self.arco_ref())
        return self

    def stop(self):
        o2lite.send_cmd(self.address + "stop", 0, "i", self.arco_ref())
        return self

    def decay(self, dur):
        o2lite.send_cmd(self.address + "decay", 0, "if",
                        self.arco_ref(), dur * self.sample_rate)
        return self

    def linear_attack(self, lin=True):
        o2lite.send_cmd(self.address + "linatk", 0, "iB",
                        self.arco_ref(), lin)
        return self

    def set(self, y):
        o2lite.send_cmd(self.address + "set", 0, "if", self.arco_ref(), y)
        return self


def envelope(env, initial_value, start, lin=False):
    if initial_value:
        env.set(initial_value)
    if start:
        env.start()
    if lin:
        env.linear_attack(lin)
    return env
