import math

# --------------- Step / frequency conversion utilities ---------------

STEP_P1 = 0.0577622650466621
STEP_P2 = 2.1011784386926213


def hz_to_step(hz):
    """Convert absolute Hz to step: 440 -> 69 (MIDI A4)."""
    return (math.log(hz) - STEP_P2) / STEP_P1


def step_to_hz(steps):
    """Convert absolute step to Hz: 69 (MIDI A4) -> 440."""
    assert isinstance(steps, (float, int))
    assert isinstance(steps * STEP_P1 + STEP_P2, float)
    assert isinstance(math.exp(steps * STEP_P1 + STEP_P2), float)
    return math.exp(steps * STEP_P1 + STEP_P2)


def step_to_ratio(steps):
    """Convert steps to frequency ratio: 7 -> ~1.5."""
    return step_to_hz(69 + steps) / 440.0


def ratio_to_step(ratio):
    """Convert frequency ratio to steps: 1.5 -> ~7."""
    return hz_to_step(ratio * 440.0) - 69


def steps_to_hzdiff(steps, delta_steps):
    """Compute change in Hz when adding delta_steps to steps."""
    return step_to_hz(steps + delta_steps) - step_to_hz(steps)

