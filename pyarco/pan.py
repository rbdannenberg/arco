import math
from ugens.mathugen import mult

# --------------- Panning utilities ---------------

def pan_linear(x, gain: float = 1):
    """Linear panning law: x=0 full left, x=1 full right."""
    x = min(1, max(0, x))
    return [(1 - x) * gain, x * gain]


def pan_eqlpow(x, gain: float = 1):
    """Equal-power panning law: L^2 + R^2 = 1 (scaled by gain)."""
    p = pan_linear(x)
    p[0] = math.sqrt(p[0]) * gain
    p[1] = math.sqrt(p[1]) * gain
    return p


def pan_45(x, gain: float = 1):
    """-4.5 dB panning law (geometric mean of linear and equal-power)."""
    x = min(1, max(0, x))
    p = pan_linear(x)
    p[0] = math.sqrt(p[0] * math.sqrt(p[0])) * gain
    p[1] = math.sqrt(p[1] * math.sqrt(p[1])) * gain
    return p


def stereoize(input, gain: float = 1):
    """apply -4.5dB panning law to a mono signal to produce a stereo signal
    with optional gain factor; terminates when input terminates."""
    return mult(input, pan_45(0.5, gain)).term()
