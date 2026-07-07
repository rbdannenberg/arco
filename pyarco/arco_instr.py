import math
import random
import threading
from collections import defaultdict

from arco import (
    Ugen,
    Const,
    get_engine,
    Smoothb,
    Const_like,
    Sum,
    Sumb,
    Route,
    Mix,
    Delay,
    Allpass,
    Lowpass,
    Sine,
    Sineb,
    Tableosc,
    Tableoscb,
    Stdistr,
    Pweb,
    Blend,
    Blendb,
    Math,
    Mathb,
    Multx,
    Addb,
    Add,
    max_chans,
    A_RATE,
    B_RATE,
    C_RATE,
    AR,
    BR,
    FADE_SMOOTH,
    BLEND_POWER,
    MATH_OP_MUL,
    MATH_OP_ADD,
    MATH_OP_SUB,
    ACTION_REM,
    ACTION_TERM,
    ACTION_END,
    ACTION_END_OR_TERM,
    MUTE,
    FINISH,
    step_to_hz,
    hz_to_step,
    steps_to_hzdiff,
    vel_to_linear,
    pan_45,
)

# ======================== Action / callback constants ========================

SIGNAL = 'signal'
GAIN = 'gain'
BOTH = 'both'

# ======================== Instrument parameter framework =====================

# Instrument-construction contexts, keyed by thread id. Each NiceGUI
# callback runs a full instr_begin() -> Instrument.__init__() sequence on
# one worker thread, so per-thread stacks stop concurrent constructions
# from cross-wiring parameters. (The active engine is deliberately a
# shared module global instead -- construction state is the exception
# because it never crosses a callback boundary.)
# CAUTION: an exception between instr_begin() and Instrument.__init__()
# orphans a context on this thread's stack; if the OS later reuses the
# thread id, the orphan can corrupt a subsequent unrelated construction.
# Callback wrappers should catch-and-clear on that boundary.
_instr_stacks = defaultdict(list)


def _instr_stack():
    return _instr_stacks[threading.get_ident()]


def instr_begin():
    """Call at the start of an Instrument subclass __init__."""
    _instr_stack().append({})


def _add_param_descr_to_context(pd, name):
    context = _instr_stack()[-1]
    if name in context:
        print("WARNING: Parameter", name, "is already specified. Ignored.")
    else:
        context[name] = pd
    return pd.value


def param(name,
          initial_value,
          op=None,
          low=None,
          high=None,
          smooth=False,
          chans=1):
    """Declare a setable parameter backed by a Const or Smoothb UGen.

    Returns the UGen so it can be wired into the instrument graph.
    """
    if isinstance(smooth,
                  (int, float)) and smooth and not isinstance(smooth, bool):
        ugen = Smoothb(initial_value, cutoff=smooth)
    elif smooth:
        ugen = Smoothb(initial_value)
    else:
        ugen = Const(initial_value)
    pd = Param_descr(ugen, initial_value, op, low, high)
    _add_param_descr_to_context(pd, name)
    return ugen


def param_map(name, initial_value, subinstr, subinstr_name=None):
    """Declare a parameter that delegates to a sub-instrument's parameter."""
    if subinstr_name is None:
        subinstr_name = name
    pd = Param_descr(subinstr, initial_value, subinstr_name, None, None)
    _add_param_descr_to_context(pd, name)


def param_method(name, initial_value, method, op=None, low=None, high=None):
    """Declare a parameter handled by calling a method on the instrument."""
    pd = Param_descr(method, initial_value, op, low, high)
    _add_param_descr_to_context(pd, name)
    return initial_value


class Param_descr:
    """Descriptor for an Instrument parameter -- links a name to a UGen or method."""

    def __init__(self, ugen_method, value, op_name=None, low=None, high=None):
        self.ugen_method = ugen_method
        self.value = self.condition(value)
        self.op_name = op_name
        self.low = low
        self.high = high

    def condition(self, x):
        if self.op_name == 'map' if hasattr(self, 'op_name') else False:
            x = self.low + (self.high - self.low) * x
        if hasattr(self, 'low') and self.low is not None:
            x = max(self.low, x)
        if hasattr(self, 'high') and self.high is not None:
            x = min(self.high, x)
        return x

    def set(self, instr, x, reuse=False):
        if isinstance(self.ugen_method, Instrument):
            self.value = x
            self.ugen_method.set(self.op_name, x)
        elif isinstance(self.ugen_method, str):
            self.value = self.condition(x)
            getattr(instr, self.ugen_method)(self.value, reuse)
        elif isinstance(self.ugen_method, Const_like):
            self.value = self.condition(x)
            self.ugen_method.set(self.value)
        return self.value


class Instrument(Ugen):
    """An Instrument wraps a UGen graph with named, setable parameters.

    Subclasses call instr_begin() at the top of __init__, build the graph,
    then call super().__init__(name, output_ugen) at the end.
    """

    def __init__(self, name, output_ugen, synth=None):
        self.synth = synth
        self.mixer_id = None
        self.user_id = None
        self.pitch = None
        self.vel = None
        self.gain = None

        if synth is not None:
            self.mixer_id = synth.get_mixer_id()

        self.output = output_ugen
        # Borrow the output ugen's id so that wiring this Instrument
        # into the graph is the same as wiring its output. The output
        # ugen owns the id; this wrapper never frees it.
        super().__init__(name,
                         output_ugen.chans,
                         output_ugen.rate,
                         "",
                         no_msg=True,
                         id_num=output_ugen.id)

        stack = _instr_stacks.get(threading.get_ident())
        if not stack:
            raise RuntimeError(
                "instr stack is empty. Did you forget instr_begin()?")
        self.parameter_bindings = stack.pop()
        if not stack:
            # don't accumulate an entry per worker thread
            del _instr_stacks[threading.get_ident()]

    def get(self, input_name):
        return self.parameter_bindings.get(input_name)

    def set(self, name, value, reuse=False):
        pd = self.parameter_bindings.get(name)
        if pd is not None:
            pd.set(self, value, reuse)

    def finish(self, status, finisher, parameters):
        if self.synth and (status & ACTION_END_OR_TERM) > 0:
            self.synth.is_finished(self)


# ======================== Note / Score =======================================


class Note:
    """A single note event in a Score."""

    def __init__(self, time, pitch, vel, dur, id=None, **params):
        self.time = time
        self.pitch = pitch
        self.vel = vel
        self.dur = dur
        self.id = id
        self.params = params

    def play(self, synth):
        synth.noteon(self.pitch,
                     self.vel,
                     dur=self.dur,
                     id=self.id,
                     pdict=self.params)

    def __repr__(self):
        return (f"Note(t={self.time}, p={self.pitch}, v={self.vel}, "
                f"d={self.dur}, id={self.id})")


class Score:
    """An ordered list of Notes with scheduled playback."""

    def __init__(self, notes=None):
        self.notes = notes or []
        self.time = 0
        self.dur = 0

    def append_rest(self, dur):
        self.dur += dur

    def merge(self, score_or_note, offset=0):
        if isinstance(score_or_note, Score):
            for note in score_or_note.notes:
                n = Note(note.time + offset, note.pitch, note.vel, note.dur,
                         note.id, **note.params)
                self.notes.append(n)
            self.notes.sort(key=lambda n: n.time)
            end = max(self.time + self.dur,
                      score_or_note.time + offset + score_or_note.dur)
            self.dur = end - self.time
        elif isinstance(score_or_note, Note):
            n = Note(score_or_note.time + offset, score_or_note.pitch,
                     score_or_note.vel, score_or_note.dur, score_or_note.id,
                     **score_or_note.params)
            self.notes.append(n)
            self.dur = max(self.dur, n.time + n.dur)
            self.notes.sort(key=lambda n: n.time)

    def append(self, score_or_note):
        offset = self.time + self.dur - (score_or_note.time if hasattr(
            score_or_note, 'time') else 0)
        self.merge(score_or_note, offset)

    def stretch(self, factor):
        for note in self.notes:
            note.time *= factor
            note.dur *= factor

    def play(self, synth):
        """Play all notes immediately (non-scheduled).

        For real-time scheduled playback, integrate with your event loop.
        """
        import time as _time
        if not self.notes:
            return
        prev_time = self.notes[0].time
        for note in self.notes:
            dt = note.time - prev_time
            if dt > 0:
                _time.sleep(dt)
            note.play(synth)
            prev_time = note.time


# ======================== Synth (polyphonic manager) =========================


def mix_name(i):
    """Generate a symbol-like string for mixer input naming."""
    return f"in{i}"


class Synth(Instrument):
    """Polyphonic synthesizer: manages creation, reuse, and removal of notes.

    Subclasses must override instr_create(note_spec, pitch, vel).
    """

    def __init__(self, instr_spec, customization, chans, param_names):
        self.instr_spec = {}
        self.param_names = list(param_names)
        if 'chans' in self.param_names:
            self.param_names.remove('chans')
        self.notes = {}
        self.free_notes = []
        self.finishing_notes = []
        self._prev_mixer_id = 0
        self.has_retrigger = False

        for p in self.param_names:
            value = customization.get(p) if customization else None
            if value is None:
                value = instr_spec.get(p)
            if value is not None:
                self.instr_spec[p] = value

        instr_begin()

        instr_chans = instr_spec.get('chans')
        chans = chans or instr_chans or 1
        instr_chans = instr_chans or chans
        self.instr_spec['chans'] = instr_chans

        output = Mix(chans)
        super().__init__("Synth", output)

    def get_mixer_id(self):
        self._prev_mixer_id += 1
        return mix_name(self._prev_mixer_id)

    def instr_create(self, note_spec, pitch, vel):
        raise NotImplementedError("Subclasses must implement instr_create")

    def noteon(self, pitch, vel, dur=None, id=None, pdict=None, **params):
        if pdict and isinstance(pdict, dict):
            pass  # use pdict as-is
        elif params:
            pdict = params
        else:
            pdict = pdict or {}

        user_id = id if id is not None else round(pitch)

        instr = self.notes.get(user_id)
        if instr is not None:
            if self.has_retrigger and hasattr(instr, 'retrigger'):
                instr.retrigger(pitch, vel, dur, pdict)
                return instr
            self.noteoff(user_id)

        if self.free_notes:
            instr = self.free_notes.pop()
            instr.user_id = user_id
            for p in self.param_names:
                value = pdict.get(p) or self.instr_spec.get(p)
                pd = instr.parameter_bindings.get(p)
                if value is not None and pd is not None and value != pd.value:
                    instr.set(p, value, True)
            instr.pitch = pitch
            instr.vel = vel
            if hasattr(instr, 'set_pitch_vel'):
                instr.set_pitch_vel()
        else:
            note_spec = dict(self.instr_spec)
            for p in self.param_names:
                v = pdict.get(p)
                if v is not None:
                    note_spec[p] = v
            instr = self.instr_create(note_spec, pitch, vel)
            instr.pitch = pitch
            instr.vel = vel

        instr.user_id = user_id
        gain_val = pdict.get('gain') or self.instr_spec.get('gain', 1)
        self.notes[user_id] = instr

        if self.chans > 1 and self.instr_spec.get('chans', 1) == 1:
            pan = pdict.get('pan') or self.instr_spec.get('pan', 0.5)
            gain_val = pan_45(pan / 127, gain_val)

        gain_const = instr.gain
        if gain_const is None:
            gain_const = Const(gain_val, None)
            instr.gain = gain_const

        if isinstance(gain_val, list):
            for i, g in enumerate(gain_val):
                gain_const.set_chan(i, g)
        else:
            gain_const.set(gain_val)

        self.output.ins(instr.mixer_id, instr, gain_const)
        return instr

    def noteoff(self, id):
        if isinstance(id, float):
            id = round(id)
        if id not in self.notes:
            print("WARNING: no note in Synth with id", id)
            return
        note = self.notes[id]
        if hasattr(note, 'noteoff'):
            note.noteoff()
        del self.notes[id]
        self.finishing_notes.append(note)

    def is_finished(self, note):
        if note in self.finishing_notes:
            self.finishing_notes.remove(note)
        elif note.user_id in self.notes:
            del self.notes[note.user_id]
        # Release the Mix input: without this the Mix pins the note's
        # entire ugen subgraph even after the note is recycled.
        self.output.rem(note.mixer_id)
        self.free_notes.append(note)

    def update_note(self, id, param_name, value):
        if param_name not in self.param_names:
            print("Synth: param", param_name, "is not setable.")
            return
        if isinstance(id, float):
            id = round(id)
        instr = self.notes.get(id)
        if instr is None:
            print("WARNING: no note in Synth with id", id)
            return
        instr.set(param_name, value)

    def update(self, param_name, value):
        if param_name not in self.param_names:
            print("Synth: param", param_name, "is not setable.")
            return
        self.instr_spec[param_name] = value
        for instr in self.notes.values():
            pd = instr.parameter_bindings.get(param_name)
            if pd is not None:
                pd.set(instr, value)


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


# ======================== Supersaw ==========================================

_sawtooth_waveforms = None


def get_sawtooth_waveforms():
    """Return the sawtooth wavetable singleton for the active engine,
    rebuilding it if the engine changed since it was created."""
    global _sawtooth_waveforms
    engine = get_engine()
    if (_sawtooth_waveforms is None
            or _sawtooth_waveforms.engine is not engine):
        _sawtooth_waveforms = Sawtooth_waveforms(engine)
    return _sawtooth_waveforms


class Sawtooth_waveforms:
    """Per-engine manager for anti-aliased sawtooth wavetables."""

    def __init__(self, engine):
        self.engine = engine
        self.created = [None] * 36
        self.tables = Tableosc(1, 1)
        self.next_index = 0

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
        instr_begin()
        self.saw = get_sawtooth_waveforms()

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

        table_index = self.saw.get_index(pitch, self.antialias != 0)

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
        comp.borrow(self.saw.tables)
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
        table_index = self.saw.get_index(self.pitch, self.antialias != 0)
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
