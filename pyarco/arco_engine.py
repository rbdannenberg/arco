import os
import sys
import time
import math
import weakref

sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), "../apps/test/python")))

ZERO_ID = 0  # a single-channel audio source of zero (silence)
ZEROB_ID = 1  # a single-channel block-rate source of zero
INPUT_ID = 2  # audio input
OUTPUT_ID = 3  # audio output (Sum ugen)
ENSEMBLE = "arco"

AR = 44100.0  # audio rate
AP = 1 / AR  # audio sample period
BL = 32
BL_RECIP = 1 / BL
BR = AR / BL
BP = 1 / BR

A_RATE = 'a'
B_RATE = 'b'
C_RATE = 'c'
NO_RATE = ''

FADE_LINEAR = 0
FADE_EXPONENTIAL = 1
FADE_LOWPASS = 2
FADE_SMOOTH = 3

MATH_OP_MUL = 0
MATH_OP_ADD = 1
MATH_OP_SUB = 2
MATH_OP_DIV = 3
MATH_OP_MAX = 4
MATH_OP_MIN = 5
MATH_OP_CLP = 6  # min(max(x, -y), y) i.e. clip if |x| > y
MATH_OP_POW = 7
MATH_OP_LT = 8
MATH_OP_GT = 9
MATH_OP_SCP = 10
MATH_OP_PWI = 11
MATH_OP_RND = 12
MATH_OP_SH = 13
MATH_OP_QNT = 14
MATH_OP_RLI = 15
MATH_OP_HZDIFF = 16
MATH_OP_TAN = 17
MATH_OP_ATAN2 = 18
MATH_OP_SIN = 19
MATH_OP_COS = 20

UNARY_OP_ABS = 0
UNARY_OP_NEG = 1
UNARY_OP_EXP = 2
UNARY_OP_LOG = 3
UNARY_OP_LOG10 = 4
UNARY_OP_LOG2 = 5
UNARY_OP_SQRT = 6
UNARY_OP_STEP_TO_HZ = 7
UNARY_OP_HZ_TO_STEP = 8
UNARY_OP_VEL_TO_LINEAR = 9
UNARY_OP_LINEAR_TO_VEL = 10
UNARY_OP_DB_TO_LINEAR = 11
UNARY_OP_LINEAR_TO_DB = 12

DNSAMPLE_BASIC = 0
DNSAMPLE_AVG = 1
DNSAMPLE_PEAK = 2
DNSAMPLE_RMS = 3
DNSAMPLE_POWER = 4
DNSAMPLE_LOWPASS500 = 5
DNSAMPLE_LOWPASS100 = 6

BLEND_LINEAR = 0
BLEND_POWER = 1
BLEND_45 = 2

ACTION_ALL = 63
ACTION_TERM = 1
ACTION_ERROR = 2
ACTION_EXCEPT = 4
ACTION_EVENT = 8
ACTION_END = 16
ACTION_REM = 32
ACTION_FREE = 64
ACTION_END_OR_TERM = ACTION_END | ACTION_TERM

MUTE = 'mute'
FINISH = 'finish'

# --------------- Step / frequency conversion utilities ---------------

STEP_P1 = 0.0577622650466621
STEP_P2 = 2.1011784386926213


def hz_to_step(hz):
    """Convert absolute Hz to step: 440 -> 69 (MIDI A4)."""
    return (math.log(hz) - STEP_P2) / STEP_P1


def step_to_hz(steps):
    """Convert absolute step to Hz: 69 (MIDI A4) -> 440."""
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


# --------------- Panning utilities ---------------

def pan_linear(x, gain=1):
    """Linear panning law: x=0 full left, x=1 full right."""
    x = min(1, max(0, x))
    return [(1 - x) * gain, x * gain]


def pan_eqlpow(x, gain=1):
    """Equal-power panning law: L^2 + R^2 = 1 (scaled by gain)."""
    p = pan_linear(x)
    p[0] = math.sqrt(p[0]) * gain
    p[1] = math.sqrt(p[1]) * gain
    return p


def pan_45(x, gain=1):
    """-4.5 dB panning law (geometric mean of linear and equal-power)."""
    x = min(1, max(0, x))
    p = pan_linear(x)
    p[0] = math.sqrt(p[0] * math.sqrt(p[0])) * gain
    p[1] = math.sqrt(p[1] * math.sqrt(p[1])) * gain
    return p


# --------------- Utilities ---------------

def max_chans(chans, ugen):
    """Compute the maximum of chans and the channels implied by ugen,
    where ugen may be a number, array, or Ugen."""
    if isinstance(ugen, (int, float)):
        return max(chans, 1)
    elif isinstance(ugen, list):
        return max(chans, len(ugen))
    else:
        return max(chans, ugen.chans)


# --------------- UgenID pool ---------------

class UgenID:

    def __init__(self, size=1000, start_id=100):
        self.size = size
        self.start_id = start_id
        self.array = [None] * size
        self.free_head = start_id  # Head of the list of free slots
        for i in range(start_id, size - 1):
            self.array[i] = i + 1  # Link each slot to the next
        self.array[size - 1] = None

    def request_slot(self):
        if self.free_head is None:
            raise Exception("No free slots available")
        slot = self.free_head
        self.free_head = self.array[slot]  # Move head to the next free slot
        self.array[slot] = None  # Mark the slot as occupied
        return slot

    def free_slot(self, index):
        if index < self.start_id or index >= self.size:
            raise IndexError("Index out of bounds")
        if self.array[index] is not None:
            raise Exception("Slot is already free")
        self.array[index] = self.free_head  # Link freed slot to current head
        self.free_head = index  # Update the head to the newly freed slot


# --- Action registration system ---

class Ugen_action:
    def __init__(self, target, method):
        self.target_ref = weakref.ref(target)  # do not pin the target
        self.method = method

    @property
    def target(self):
        return self.target_ref()

    def __repr__(self):
        return f"<Ugen_action {self.target} {self.method!r}>"


class Action_list:
    def __init__(self, action_mask, ugen_actions):
        self.action_mask = action_mask
        self.ugen_actions = ugen_actions


# --------------- Active engine accessor ---------------

_active_engine = None


def get_engine():
    """Return the active ArcoEngine."""
    if _active_engine is None:
        raise RuntimeError(
            "No active ArcoEngine -- call engine.connect() first")
    return _active_engine


# --------------- ArcoEngine ---------------

class ArcoEngine:
    """Owns the O2lite connection, UgenID pool, and registry of live ugens."""

    def __init__(self, input_chans=2, output_chans=2, ensemble="arco",
                 timeout=30):
        self.input_chans = input_chans
        self.output_chans = output_chans
        self.ensemble = ensemble
        self.timeout = timeout
        self.o2lite = None
        self.id_pool = UgenID()
        self._ugens = weakref.WeakValueDictionary()  # id -> Ugen (weak)
        self.action_dict = {}       # action_id -> Action_list
        self.next_action_id = 1
        self.fade_in_lookup = {}    # ugen.id -> Fader
        # System ugen references (set during connect)
        self.zero = None
        self.zerob = None
        self.input = None
        self.output = None

    def connect(self):
        """Connect to Arco server, create system ugens, set as active engine."""
        if self.o2lite is not None:
            return  # already connected
        from arco_ugens import Ugen, Sum  # deferred to break circular import
        from o2lite import O2lite  # deferred so the library imports without o2litepy

        self.o2lite = O2lite()
        self.o2lite.initialize(self.ensemble, debug_flags="a")
        deadline = time.time() + self.timeout
        while self.o2lite.time_get() < 0:
            if time.time() > deadline:
                self.o2lite = None
                raise TimeoutError(
                    f"Could not connect to Arco server within "
                    f"{self.timeout} seconds. Is arcobasic running?")
            self.o2lite.poll()
            time.sleep(0.01)
        print("Connected to ensemble", self.ensemble,
              "O2time", self.o2lite.time_get())

        # Reset Arco — the host creates system ugens at IDs 0-3 as Thru's.
        self.o2lite.send_cmd("/arco/reset", 0, "")
        for _ in range(100):
            self.o2lite.poll()
            time.sleep(0.01)

        # Set as active engine BEFORE creating system ugens
        global _active_engine
        _active_engine = self

        # System ugen shadows (no_msg=True -- host already created them).
        # Use id_num to avoid wasting pool slots.
        self.zero = Ugen("Zero", 1, A_RATE, "", no_msg=True,
                         engine=self, id_num=ZERO_ID)
        self.zerob = Ugen("Zerob", 1, B_RATE, "", no_msg=True,
                          engine=self, id_num=ZEROB_ID)
        self.input = Ugen("Thru", self.input_chans, A_RATE, "", no_msg=True,
                          engine=self, id_num=INPUT_ID)

        # Replace host's Thru at OUTPUT_ID with a Sum (play/mute need it).
        self.o2lite.send_cmd("/arco/free", 0, "i", OUTPUT_ID)
        self.output = Sum(self.output_chans, True, OUTPUT_ID)

        print("Arco initialized: system ugens created")

    def close(self):
        """Free all ugens, disconnect, and clear active engine."""
        if self.o2lite is None:
            return
        # Free pool-allocated ugens in reverse id order
        for ugen_id, ugen in sorted(list(self._ugens.items()), reverse=True):
            self.send_cmd("/arco/free", 0, "i", ugen_id)
            ugen.engine = None  # prevent double-free in __del__
        self._ugens.clear()
        for ugen in (self.zero, self.zerob, self.input, self.output):
            if ugen is not None:
                ugen.engine = None
        self.id_pool = UgenID()
        self.action_dict.clear()
        self.next_action_id = 1
        self.fade_in_lookup.clear()
        self.zero = None
        self.zerob = None
        self.input = None
        self.output = None
        self.o2lite = None
        global _active_engine
        if _active_engine is self:
            _active_engine = None

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *exc):
        self.close()

    def send_cmd(self, *args):
        """Send an O2 command, guarding against disconnected state."""
        if self.o2lite is not None:
            self.o2lite.send_cmd(*args)

    def poll(self):
        """Poll the O2lite connection."""
        if self.o2lite is not None:
            self.o2lite.poll()

    def register(self, ugen):
        """Track a pool-allocated ugen. References are weak: a ugen lives
        as long as user code or the client-side graph references it."""
        self._ugens[ugen.id] = ugen

    def unregister(self, ugen_id):
        """Remove a ugen from the registry."""
        self._ugens.pop(ugen_id, None)

    def register_action(self, ugen, action_mask, target, method):
        """Register a callback action for ugen events."""
        action = Ugen_action(target, method)
        aid = ugen.action_id
        if aid is not None:
            action_list = self.action_dict.get(aid)
            if action_list is None:
                print("ERROR: register_action - action_id not in action_dict",
                      aid)
                return
            if action_mask != (action_mask & action_list.action_mask):
                action_list.action_mask = action_list.action_mask | action_mask
                self.send_cmd("/arco/act", 0, "iii", ugen.id, aid,
                              action_list.action_mask)
            action_list.ugen_actions.append(action)
        else:
            al = Action_list(action_mask, [action])
            self.action_dict[self.next_action_id] = al
            ugen.action_id = self.next_action_id
            self.send_cmd("/arco/act", 0, "iii", ugen.id,
                          self.next_action_id, action_mask)
            self.next_action_id += 1

    def actl_act_handler(self, timestamp, address, types, key, status, uid):
        """Handler for /actl/act messages from Arco server."""
        al = self.action_dict.get(key)
        if al is None:
            return
        if status & ACTION_FREE:
            self.action_dict.pop(key, None)
            return
        live = []
        for ua in al.ugen_actions:
            target = ua.target
            if target is None:
                continue  # target was garbage-collected; drop the action
            live.append(ua)
            if status & al.action_mask and hasattr(target, ua.method):
                try:
                    getattr(target, ua.method)(status)
                except Exception as e:
                    print("ERROR: actl_act_handler -", ua.method,
                          "callback raised:", repr(e))
        al.ugen_actions = live
