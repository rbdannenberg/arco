"""Rate-guard behavior for block-rate and audio-rate ugen constructors.

Ground truth is Arco's own type system (arco/preproc/u2f.py): a ``.ugen`` input
declared ``b`` expands to ``"bc"`` (block-rate *or* c-rate/Const accepted), and
one declared ``a`` stays audio-rate only. The generated Serpent wrappers agree
(mathugen.srp: ``Mathb`` uses ``"bc"``, ``Math`` uses ``"abc"``).

So block-rate constructors must accept numbers, Const (c-rate), and b-rate
ugens, rejecting only a-rate; audio-rate ``snd`` inputs must require a-rate.
Rejection raises ``ValueError`` rather than printing and returning a
half-constructed object.
"""

import pytest

from arco_engine import A_RATE, B_RATE, MATH_OP_ADD, UNARY_OP_ABS
from arco_ugens import (
    Const, Sine, Sineb, Sumb, Reson, Resonb, Mathb, Unaryb, Lowpass, Tableoscb,
)


def a_rate_source():
    """An audio-rate ugen (Sine outputs at A_RATE)."""
    return Sine(440, 0.5)


def b_rate_source():
    """A block-rate ugen (Sumb outputs at B_RATE)."""
    return Sumb(1)


# Each builder places `candidate` in a rate-checked slot; other inputs are
# plain numbers (always valid — they auto-wrap to a Const).
BLOCK_RATE_BUILDERS = {
    "Sineb": lambda c: Sineb(c, 0.5),
    "Resonb": lambda c: Resonb(c, 500, 4),
    "Mathb": lambda c: Mathb(MATH_OP_ADD, c, 1),
    "Unaryb": lambda c: Unaryb(UNARY_OP_ABS, c),
    "Tableoscb": lambda c: Tableoscb(c, 0.5),
}

AUDIO_RATE_BUILDERS = {
    "Reson": lambda c: Reson(c, 500, 4),
    "Lowpass": lambda c: Lowpass(c, 2000),
}


# ---------------------------------------------------------------------------
# Block-rate constructors: accept number / Const (c-rate) / b-rate; reject a.
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", BLOCK_RATE_BUILDERS)
def test_block_rate_accepts_const(engine, name):
    ugen = BLOCK_RATE_BUILDERS[name](Const(1.0))
    assert ugen.id is not None
    assert ugen.rate == B_RATE


@pytest.mark.parametrize("name", BLOCK_RATE_BUILDERS)
def test_block_rate_accepts_number(engine, name):
    ugen = BLOCK_RATE_BUILDERS[name](7.0)
    assert ugen.id is not None
    assert ugen.rate == B_RATE


@pytest.mark.parametrize("name", BLOCK_RATE_BUILDERS)
def test_block_rate_accepts_b_rate_ugen(engine, name):
    ugen = BLOCK_RATE_BUILDERS[name](b_rate_source())
    assert ugen.id is not None
    assert ugen.rate == B_RATE


@pytest.mark.parametrize("name", BLOCK_RATE_BUILDERS)
def test_block_rate_rejects_a_rate_ugen(engine, name):
    with pytest.raises(ValueError):
        BLOCK_RATE_BUILDERS[name](a_rate_source())


# ---------------------------------------------------------------------------
# Audio-rate constructors: `snd` must be audio rate; reject b-rate / Const.
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", AUDIO_RATE_BUILDERS)
def test_audio_rate_accepts_a_rate_ugen(engine, name):
    ugen = AUDIO_RATE_BUILDERS[name](a_rate_source())
    assert ugen.id is not None
    assert ugen.rate == A_RATE


@pytest.mark.parametrize("name", AUDIO_RATE_BUILDERS)
def test_audio_rate_rejects_b_rate_ugen(engine, name):
    with pytest.raises(ValueError):
        AUDIO_RATE_BUILDERS[name](b_rate_source())


@pytest.mark.parametrize("name", AUDIO_RATE_BUILDERS)
def test_audio_rate_rejects_const(engine, name):
    with pytest.raises(ValueError):
        AUDIO_RATE_BUILDERS[name](Const(1.0))
