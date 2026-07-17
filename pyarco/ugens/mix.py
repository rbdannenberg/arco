from arco_ugens import *

class Mix(Ugen):

    def __init__(self, chans=1, wrap=True):
        super().__init__(new_ugen_id(), "Mix", chans, A_RATE, "i", None, None,
                         'wrap', 1 if wrap else 0)

    def ins(self, name, ugen, gain, dur=0, mode=FADE_SMOOTH):
        if isinstance(gain, (int, float, list)):
            gain = Const(gain, None)
        elif gain.rate == A_RATE:
            print("WARNING: In Mix.ins, audio-rate mix gain is not allowed.")
            return self
        self.inputs[name] = [name, ugen, gain]
        o2lite.send_cmd("/arco/mix/ins", 0, "isiifi", self.arco_ref(), str(name),
                        ugen.arco_ref(), gain.arco_ref(), dur, mode)
        return self

    def rem(self, name, dur=0, mode=FADE_SMOOTH):
        if name in self.inputs:
            o2lite.send_cmd("/arco/mix/rem", 0, "isfi", self.arco_ref(),
                            str(name), dur, mode)
            del self.inputs[name]
        return self

    def find_name_of(self, ugen):
        for key, val in self.inputs.items():
            if isinstance(val, list) and val[1] == ugen:
                return key
        return None

    def set_gain(self, name, gain, chan=0):
        if name not in self.inputs:
            print("ERROR: Mix.set_gain() cannot find input", name)
            return self
        entry = self.inputs[name]
        gain_ugen = entry[2]
        if isinstance(gain_ugen, Ugen) and gain_ugen.rate == C_RATE:
            if chan >= gain_ugen.chans:
                print("WARNING: In Mix.set_gain(), gain for", name, "is a",
                      gain_ugen.chans, "channel Const, but set_gain requests"
                      " channel", chan)
            o2lite.send_cmd("/arco/mix/set_gain", 0, "isif", self.arco_ref(),
                            str(name), chan, gain)
        else:
            if isinstance(gain, (int, float, list)):
                gain = Const(gain, None)
            elif gain.rate == A_RATE:
                print("WARNING: In Mix.set_gain(), audio-rate mix gain"
                      " will be downsampled and then interpolated")
            entry[2] = gain
            o2lite.send_cmd("/arco/mix/repl_gain", 0, "isi", self.arco_ref(),
                            str(name), gain.arco_ref())
        return self

    def set_in(name, input):
    # general set input to an a-rate Ugen
        if not inputs.has_key(name):
            print("ERROR: Mix.set_in() cannot find input", name)
            return self
        in_ugen = inputs[name][1]
        if input.rate != 'a':
            print("ERROR: In Mix.set_in(), new input for", repr(name),
                  "has non-audio-rate", repr(input.rate),
                  "- Input not replaced.")
            return self
        inputs[name][1] = input
        o2_send_cmd("/arco/mix/repl_in", 0, "UsU", id, str(name),
                    input.arco_ref())
        return self


def mix(chans = 1, wrap=True):
    return Mix(chans, wrap)


def mix_name(i):
    """handy function to convert index into a symbol to name an input when
    inputs are originally created in a loop or come from an array"""
    return "in" + str(i)

