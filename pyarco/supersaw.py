

# ======================== Supersaw ==========================================

_sawtooth_waveforms = None


class Sawtooth_waveforms:
    """Singleton manager for anti-aliased sawtooth wavetables."""

    def __init__(self):
        global _sawtooth_waveforms
        self.created = [None] * 36
        self.tables = Tableosc(1, 1)
        self.next_index = 0
        _sawtooth_waveforms = self

    def get_index(self, step, antialias=True):
        if antialias:
            i = max(min(int(step / 4), len(self.created) - 1), 1)
        else:
            i = 0
        if self.created[i] is not None:
            return self.created[i]
        self.created[i] = self.next_index

        if antialias:
            f0 = step_to_hz(step)
            n = max(1, int(AR * 2 / (5 * f0)))
            tlen = max(16 * n, 512)
            ampspec = [1 / (j + 1) for j in range(n)]
            self.tables.create_tas(self.next_index, tlen, ampspec)
        else:
            ampspec = [i / 256 for i in range(256)]
            self.tables.create_ttd(self.next_index, ampspec)

        self.next_index += 1
        return self.next_index - 1


class Supersaw_instr(Instrument):
    """A supersaw instrument with multiple detuned sawtooth oscillators."""

    def __init__(self, synth, note_spec, pitch, vel):
        global _sawtooth_waveforms
        instr_begin()

        if _sawtooth_waveforms is None:
            Sawtooth_waveforms()

        chans = note_spec.get('chans', 1)
        self.n = max(1, round(note_spec.get('n', 8)))
        self.rolloff = note_spec.get('rolloff', 0)
        self.animate = note_spec.get('animate', 0)
        self.decay_time = max(0, note_spec.get('decay', 0.1))
        self.antialias = note_spec.get('antialias', 1)
        rndphase = note_spec.get('rndphase', 1)
        self.pitch = pitch
        self.vel = vel
        self.recalc_tableosc_amps = False

        param_method('animate', self.animate, 'set_animate', 'clip', 0, 1)
        self.animate_const = Const(self.animate)
        param_method('antialias', self.antialias, 'set_antialias', 'clip', 0,
                     1)
        param_method('rolloff', self.rolloff, 'set_rolloff', 'clip', 0, 1)

        table_index = _sawtooth_waveforms.get_index(pitch, self.antialias != 0)

        if chans == 1:
            self.mixer = Sum(1)
        else:
            width = note_spec.get('width', 0)
            param_method('width', width, 'set_width', 'clip', 0, 1)
            self.mixer = Stdistr(self.n, width)

        attack = note_spec.get('attack', 0.04)
        param_method('attack', attack, 'set_attack', 'clip', 0, None)
        self.env = Pweb([attack, vel_to_linear(vel)])
        self.env.linear_attack()

        cutoff = note_spec.get('cutoff', 100)
        param_method('cutoff', cutoff, 'set_cutoff')
        lphz = min(AR * 0.4, step_to_hz(cutoff + pitch))
        self._lowpass = Lowpass(self.mixer, lphz)
        output = Math.mult(self._lowpass, self.env)

        self.pitch_const = Const(pitch)
        lfofreq = note_spec.get('lfofreq', 5)
        lhz = param('lfofreq', lfofreq, 'map', 0, 20)
        lfodepth = note_spec.get('lfodepth', 0)
        ldp = param('lfodepth', lfodepth, 'map', 0, 2)
        self.vib = Sineb(lhz, Mathb(MATH_OP_SUB, self.pitch_const, ldp))

        self.shared_hz = Addb()
        self.shared_hz.ins(Const(step_to_hz(pitch)))
        self.shared_hz.ins(self.vib)

        detune = note_spec.get('detune', 0)
        dhz = param('detune', detune, 'map', 0, 2)
        self.detune_hz = Mathb(MATH_OP_SUB, self.pitch_const, dhz)

        anirate = note_spec.get('anirate', 1)
        self.initial_detune = self.animate * steps_to_hzdiff(pitch, detune)
        self.animate_detune = Mathb(MATH_OP_MUL, self.animate_const,
                                    self.detune_hz)
        self.anirate_const = param('anirate', anirate, 'map', 0, 10)

        self._calc_gain_to_normalize()
        self.components = [
            self._supersaw_component(i, chans, table_index, rndphase)
            for i in range(self.n)
        ]

        super().__init__("Supersaw", output, synth)

    def _supersaw_component(self, i, chans, table_index, rndphase):
        detune_frac = (i * 2) / (self.n - 1) - 1 if self.n > 1 else 0
        fixed_fmod = Mathb(MATH_OP_MUL, self.detune_hz, Const(detune_frac))

        hz = Addb()
        hz.ins(self.shared_hz)
        blend_ugen = Blendb(fixed_fmod, self.animate_detune,
                            self.animate_const, BLEND_POWER)
        hz.ins(blend_ugen)

        amp = self._calc_tableosc_amp(i)
        phase = rndphase * random.uniform(0, 360)
        comp = Tableosc(hz, Const(amp), phase=phase)
        comp.borrow(_sawtooth_waveforms.tables)
        comp.select(table_index)

        if chans == 1:
            self.mixer.ins(comp)
        else:
            self.mixer.ins(i, comp)
        return comp

    def _calc_tableosc_amp(self, i):
        k = abs(i - (self.n - 1) / 2)
        return ((self.animate + (1 - self.animate) * (self.rolloff**k)) *
                self.gain_to_normalize)

    def _calc_gain_to_normalize(self):
        rsqr = self.rolloff**2
        power = 1
        if self.rolloff == 1:
            power += (self.n - 1)
        elif self.n > 1:
            power += 2 * rsqr * (1 - self.rolloff**
                                 (self.n - 1)) / (1 - rsqr) if rsqr != 1 else 0
        power = self.n * self.animate + power * (1 - self.animate)
        self.gain_to_normalize = 1 / math.sqrt(max(power, 0.001))

    def _calc_tableosc_amps(self):
        self._calc_gain_to_normalize()
        for i, comp in enumerate(self.components):
            amp = self._calc_tableosc_amp(i)
            comp.set('amp', amp)

    def set_rolloff(self, rolloff_, reuse=False):
        self.rolloff = rolloff_
        if reuse:
            self.recalc_tableosc_amps = True
        else:
            self._calc_tableosc_amps()

    def set_attack(self, attack_, reuse=False):
        self.attack = attack_

    def set_animate(self, animate_, reuse=False):
        self.animate = animate_
        self.animate_const.set(animate_)
        if reuse:
            self.recalc_tableosc_amps = True
        else:
            self._calc_tableosc_amps()

    def set_cutoff(self, cutoff, reuse=False):
        self._lowpass.set('cutoff',
                          min(AR * 0.4, step_to_hz(cutoff + self.pitch)))

    def set_antialias(self, antialias_, reuse=False):
        self.antialias = antialias_
        if not reuse:
            self._calc_tableosc_index()

    def set_width(self, width, reuse=False):
        if self.chans == 2:
            self.mixer.set_width(width)

    def _calc_tableosc_index(self):
        table_index = _sawtooth_waveforms.get_index(self.pitch, self.antialias
                                                    != 0)
        for comp in self.components:
            comp.select(table_index)

    def set_pitch_vel(self):
        if self.recalc_tableosc_amps:
            self._calc_tableosc_amps()
            self.recalc_tableosc_amps = False
        self.pitch_const.set(self.pitch)
        self.shared_hz.set('x1', step_to_hz(self.pitch))
        self._calc_tableosc_index()
        self.env.set_points(self.attack if hasattr(self, 'attack') else 0.04,
                            vel_to_linear(self.vel))
        self.env.start()

    def noteoff(self):
        self.env.decay(self.decay_time)


class Supersaw_synth(Synth):
    """A Synth that creates Supersaw_instr instances."""

    def __init__(self, instr_spec, customization=None, chans=None):
        super().__init__(instr_spec, customization or {}, chans, [
            'animate', 'anirate', 'antialias', 'n', 'rndphase', 'detune',
            'width', 'rolloff', 'attack', 'decay', 'cutoff', 'lfofreq',
            'lfodepth'
        ])

    def instr_create(self, note_spec, pitch, vel):
        return Supersaw_instr(self, note_spec, pitch, vel)
