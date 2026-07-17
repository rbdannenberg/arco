# ======================== Synth (polyphonic manager) =========================

_mix_name_counter = 0


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
