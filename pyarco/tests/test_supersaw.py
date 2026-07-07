"""End-to-end construction of a Supersaw voice.

Supersaw_instr builds block-rate modulation from Const inputs (e.g.
``Mathb(MATH_OP_SUB, self.pitch_const, ldp)``), so it exercises the exact
rate-guard path that used to reject c-rate Const inputs. This test drives a full
``noteon``/``noteoff`` cycle offline to guard against regressions.
"""

from arco_instr import Supersaw_synth


def test_supersaw_noteon_noteoff_cycle(engine):
    # get_sawtooth_waveforms() is engine-scoped and rebuilds for a new active
    # engine, so the fresh per-test engine gets its own wavetable singleton.
    synth = Supersaw_synth({})

    instr = synth.noteon(60, 100)
    assert instr is not None
    assert instr.id is not None
    # A default supersaw has n detuned oscillator components.
    assert len(instr.components) == instr.n
    assert instr.n >= 1

    synth.noteoff(60)
    assert 60 not in synth.notes


def test_supersaw_multiple_voices(engine):
    synth = Supersaw_synth({})
    a = synth.noteon(60, 100)
    b = synth.noteon(64, 90)

    assert a.id is not None and b.id is not None
    assert a.id != b.id

    synth.noteoff(60)
    synth.noteoff(64)
    assert synth.notes == {}
