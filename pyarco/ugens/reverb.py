
# ======================== Reverb =============================================

REVERB_COMBDELAY = [
    0.028049887, 0.031315193, 0.036439909, 0.040294785, 0.044195011,
    0.046780045
]

REVERB_ALLPASSDELAY = [
    1 / hz for hz in
    [143.6482085, 454.6391753, 621.1267606, 832.0754717, 1422.580645]
]

REVERB_DECAY = [0.822, 0.802, 0.773, 0.753, 0.753, 0.753]


class Reverb(Instrument):
    """Simple mono comb+allpass reverb (ported from Nyquist REVERB-MONO)."""

    def __init__(self, input, rt60, wet=1, hz=10000, chans=None):
        instr_begin()
        self.combs = []
        self.allpasses = []
        rvb = Sum(1)
        for i, delay_t in enumerate(REVERB_COMBDELAY):
            fb = math.exp(-6.9087 * delay_t / (rt60 * REVERB_DECAY[i]))
            c = Delay(input, delay_t, fb, delay_t + 0.001)
            self.combs.append(c)
            rvb.ins(c)

        for i, delay_t in enumerate(REVERB_ALLPASSDELAY):
            fb = math.exp(-6.9087 * delay_t / (rt60 * 0.7))
            rvb_next = Allpass(rvb, delay_t, fb, delay_t + 0.001)
            self.allpasses.append(rvb_next)
            rvb = rvb_next

        self._lowpass = Lowpass(rvb, min(hz, AR * 0.4))

        if wet != 1:
            output = Mix(1)
            output.ins('dry', input, Const(1 - wet))
            output.ins('wet', self._lowpass, Const(wet))
        else:
            output = self._lowpass

        super().__init__("Reverb", output)

    def set_input(self, input):
        for cb in self.combs:
            cb.set('input', input)

    def set_rt60(self, rt60):
        for i, cb in enumerate(self.combs):
            fb = math.exp(-6.9087 * REVERB_COMBDELAY[i] /
                          (rt60 * REVERB_DECAY[i]))
            cb.set('fb', fb)
        for i, ap in enumerate(self.allpasses):
            fb = math.exp(-6.9087 * REVERB_ALLPASSDELAY[i] / (rt60 * 0.7))
            ap.set('fb', fb)

    def set_cutoff(self, freq):
        self._lowpass.set('cutoff', freq)


def reverb(input, rt60, wet=1, hz=10000):
    """Create a mono Reverb instrument."""
    return Reverb(input, rt60, wet, hz)


class Multi_reverb(Instrument):
    """Stereo (or multi-channel) reverb using one or two Reverb instances."""

    def __init__(self, input, rt60, wet=1, hz=10000, chans=None):
        chans = chans or input.chans
        self.wet = wet
        self._input = input
        self.dry_input = None
        self.reverbs = []
        self.reverb_inputs = []
        instr_begin()
        self._multi_init(input, rt60, wet, hz, chans)
        super().__init__("Multi_reverb", self.output)

    def _multi_init(self, input, rt60, wet, hz, chans_):
        left_route = Route(1)
        self.reverb_inputs = [left_route]

        if chans_ > 1:
            right_route = Route(1)
            self.reverb_inputs.append(right_route)

        self._connect_input(input)

        left_rev = Reverb(left_route, rt60, 1, hz)
        self.reverbs = [left_rev]
        right_rev = None
        if chans_ > 1:
            right_rev = Reverb(right_route, rt60 * 1.1, 1, hz * 1.1)
            self.reverbs.append(right_rev)

        self.output = Route(chans_)

        if wet != 1:
            left_wet = Math.mult(left_rev, wet)
            if chans_ > 1:
                right_wet = Math.mult(right_rev, wet)
            self._connect_dry_inputs(input, chans_)
            left_rev = left_wet
            if chans_ > 1:
                right_rev = right_wet

        if chans_ > 1:
            for i in range(chans_):
                if i % 2 == 0:
                    self.output.ins(left_rev, 0, i)
                else:
                    self.output.ins(right_rev, 0, i)
        else:
            self.output.ins(left_rev, 0, 0)

    def _connect_input(self, new_input):
        left_input = self.reverb_inputs[0]
        if len(self.reverb_inputs) == 1:
            for i in range(new_input.chans):
                left_input.ins(new_input, i, 0)
        else:
            right_input = self.reverb_inputs[1]
            for i in range(new_input.chans):
                if i % 2 == 0:
                    left_input.ins(new_input, i, 0)
                else:
                    right_input.ins(new_input, i, 0)
            if new_input.chans == 1:
                right_input.ins(new_input, 0, 0)

        if self.reverbs:
            self.reverbs[0].set_input(left_input)
            if len(self.reverbs) > 1:
                self.reverbs[1].set_input(right_input)

    def _connect_dry_inputs(self, new_input, chans_):
        dry_gain = 1 - self.wet
        if self.dry_input is not None:
            self.output.reminput(self.dry_input)
        self.dry_input = Math.mult(new_input, dry_gain)
        if new_input.chans == 1:
            for i in range(chans_):
                self.output.ins(self.dry_input, 0, i)
        else:
            for i in range(new_input.chans):
                self.output.ins(self.dry_input, i, i % chans_)

    def set_rt60(self, rt60):
        for r in self.reverbs:
            r.set_rt60(rt60)

    def set_cutoff(self, freq):
        for r in self.reverbs:
            r.set_cutoff(freq)

    def set_input(self, new_input):
        self._connect_input(new_input)
        if self.wet != 1:
            self._connect_dry_inputs(new_input, self.output.chans)
        self._input = new_input


def multi_reverb(input, rt60, wet=1, hz=10000, chans=None):
    """Create a multi-channel Reverb instrument."""
    chans = chans or input.chans
    return Multi_reverb(input, rt60, wet, hz, chans)
