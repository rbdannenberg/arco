import math

# --------------- Velocity / dB conversion utilities ---------------

_LOG_OF_10_OVER_20 = math.log(10.0) / 20.0


def db_to_linear(x):
    """Convert dB to linear amplitude."""
    return math.exp(_LOG_OF_10_OVER_20 * x)


def linear_to_db(x):
    """Convert linear amplitude to dB."""
    return math.log(x) / _LOG_OF_10_OVER_20


def vel_to_linear(v):
    """Convert MIDI velocity to linear amplitude."""
    return ((v * 0.00768553) + 0.0239372) ** 2


def linear_to_vel(x, use_float=False):
    """Convert linear amplitude to MIDI velocity."""
    x = (math.sqrt(abs(x)) - 0.0239372) / 0.00768553
    if not use_float:
        x = max(1, min(127, round(x)))
    return x


def vel_to_db(v):
    """Convert MIDI velocity to dB."""
    return linear_to_db(vel_to_linear(v))


def db_to_vel(x, use_float=False):
    """Convert dB to MIDI velocity."""
    return linear_to_vel(db_to_linear(x), use_float)
