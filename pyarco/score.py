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

