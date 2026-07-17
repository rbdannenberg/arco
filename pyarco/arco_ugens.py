import sched  # for globals and more
from sched import rtsched, vtsched, absolute, real_delay
from typing import Optional
from o2litepy import o2lite


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

# --------------- Utilities ---------------


_arco_callbacks = []

def need_arco_reference(fn):
    """Register for a callback to fn with the value of the
    singleton arco"""
    _arco_callbacks.append(fn)


def arco_ugens_initialize(arco_ref):
    """Complete a circular dependency -- we would like to simply
    from arco_engine import arco, but we cannot because arco_engine
    imports this module. This simulates an import of arco after
    arco_engine is running and the client calls arco.initialize()

    Unfortunately, other modules (e.g., sum) may need arco as well.
    They should call need_arco_reference(fn) with a callback that
    will receive arco_ref.
    """
    global arco
    # print("arco_ugens_initialize called with", arco_ref)
    arco = arco_ref
    for callback in _arco_callbacks:
        callback(arco_ref)


def max_chans(chans, *ugens):
    """Compute the maximum of chans and the channels implied by ugens,
    where ugens may be numbers, arrays, or Ugens."""
    if chans:
        return chans
    chans = 1
    for ugen in ugens:
        if isinstance(ugen, Param_descr):
            ugen = ugen.value()
        chans = max(chans, 1 if isinstance(ugen, (int, float)) else
                           len(ugen) if isinstance(ugen, list) else ugen.chans)
    return chans



# --------------- UgenID pool ---------------

class UgenID:

    def __init__(self, size=1000, start_id=100):
        self.size = size
        self.start_id = start_id
        self.array : list[Optional[int]] = [None] * size
        self.arco_epoch = 0  # Epoch for unique ID generation
        self.new_epoch()  # initial epoch is 1 << 32


    def new_epoch(self):
        self.free_head = self.start_id  # Head of the list of free slots
        for i in range(self.start_id, self.size - 1):
            self.array[i] = i + 1  # Link each slot to the next
        self.array[self.size - 1] = None
        self.arco_epoch += (1 << 32)
        

    def new_ugen_id(self, id_num=None):
        if self.free_head is None:
            raise Exception("No free slots available")
        if id_num is not None:
            if id_num < 0 or id_num >= self.start_id:
                raise ValueError(f"Invalid fixed id_num {id_num}.")
            return id_num | self.arco_epoch
        slot = self.free_head
        self.free_head = self.array[slot]  # Move head to the next free slot
        self.array[slot] = None  # Mark the slot as occupied
        return slot | self.arco_epoch

    def free_ugen_id(self, index):
        if index < self.start_id or index >= self.size:
            raise IndexError("Index out of bounds")
        if self.array[index] is not None:
            raise Exception("Slot is already free")
        self.array[index] = self.free_head  # Link freed slot to current head
        self.free_head = index  # Update the head to the newly freed slot


arco_ids = UgenID()

def new_ugen_id(id_num=None):
    return arco_ids.new_ugen_id(id_num)



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


fade_in_lookup = {}  # maps ugen id to fader ugen for fade_in


class Ugen:

    def __init__(
        self,
        id_num,
        classname_,
        chans_,
        rate_,
        types_,
        no_msg=None,
        omit_chans=None,
        *inputs_
    ):
        inputs_ = list(inputs_)
        self.id_num = id_num
        self.classname = classname_
        self.chans = chans_
        self.rate = rate_
        self.inputs = {}  # mapping from input name (symbol) to a ugen
        # in the case of Mix and perhaps others, inputs is an array
        # containing an input name, input ugen and gain ugen
        self.action_id = None

        if no_msg:
            return

        # calling const() if needed will send a message, so
        # we coerce all numbers to const() BEFORE constructing
        # the /arco/*/new message:
        for i in range(1, len(inputs_), 3):
            inpi = inputs_[i]  # actual input value
            if isinstance(inpi, Param_descr):  # update with info
                inpi.add_ugen_user(this, inputs_[i - 1], inputs_[i + 1])
                inpi = inpi.get_ugen_value(classname, inputs_[i - 1])
                inputs_[i] = inpi
            # if input is (still) float or array, convert to Const Ugen
            if types_[i // 3] == "U" and (isinstance(inputs_[i], (int, float))
                                          or isinstance(inputs_[i], list)):
                inputs_[i] = Const(inputs_[i], chans_)
        # construct the message
        address = f"/arco/{self.classname.lower()}/new"
        params = [self.arco_ref()]  # first parameter is the ugen id
        type_str = "i"  # only the id at first

        if not omit_chans:  # some Ugens do not have chans parameter
            params.append(self.chans)
            type_str += "i"

        for i in range(0, len(inputs_), 3):
            inp = inputs_[i + 1]  # the value
            if types_[i // 3] == "U":  # it's a Ugen
                params.append(inp.arco_ref())
                self.inputs[inputs_[i]] = inp  # build inputs dict
                type_str += "i"
            else:  # one of "sihdft"
                params.append(inp)
                type_str += types_[i // 3]
        # send "/arco/<classname>/new" message to arco
        o2lite.send_cmd(address, 0, type_str, *params)
        # print(f"New {self.classname} id {self.arco_ref()}")


    def __del__(self):
        if arco_ids.arco_epoch != (self.id_num >> 32):
            return  # residual Ugen from another epoch. All Ugens in that
                    # epoch were freed.
        ar = self.arco_ref()
        o2lite.send_cmd("/arco/free", 0, "i", ar)
        # print("__del__ freeing", ar)
        if ar >= arco_ids.start_id:
            arco_ids.free_ugen_id(ar)


    def __str__(self):
        return (f"<{str(self.classname)} Ugen, id {self.arco_ref()}"
                f" chans {self.chans}>")


    def arco_ref(self):
        return self.id_num & 0xFFFFFFFF


    def play(self):
        arco.output.ins(self)
        return self


    def mute(self, status=None):
        # status is accepted (passed by atend actions) but ignored here
        arco.output.rem(self)


    def fade(self, dur, mode=FADE_SMOOTH):
        """Fade output to zero over dur seconds, then disconnect."""
        fader = fade_in_lookup.get(self.id_num)
        if fader:  # fade_in is in progress; convert to fade out
            fader.set_dur(dur)
            fader.set_goal(0)
            fader.set_mode(mode)
            return fader
        else:
            faded = fader(self, 1, dur, 0).term()
            arco.output.swap(self, faded)
            faded.set_mode(mode)
            return faded  # return the new fader ugen


    def fade_in(self, dur, mode=FADE_SMOOTH, term=True):
        """Fade in from silence. Ugen must NOT already be playing.

        fade from 0 to 1, then disconnect fader. This ugen must
        *not* be connected to output ugen (i.e. .play() has not
        been called. On return, this ugen will be connected
        through a fader to output ugen and starting to fade in.
        After the fade-in, the fader will be removed and this
        ugen will be connected directly to output ugen. During
        the fade-in or after the fade-in, you can call the
        fade method to fade back out and remove this ugen from
        the output ugen.
        """
        fader = fader(self, 0, dur, 1)
        if term:
            fader.term()
        fade_in_lookup[self.id] = fader
        fader.set_mode(mode)
        fader.play()
        sched.cause(dur + 0.1, fader, 'fade_in_complete', self)

    def fade_in_complete(self):
        f = fade_in_lookup.get(self.id)
        if f is not None:
            # swap fader out, put the original ugen directly in output
            arco.output.swap(self, f)
            del fade_in_lookup[self.id]


    def run(self):  # add this Ugen to the run set
        o2lite.send_cmd("/arco/run", 0, "i", self.arco_ref())


    def unrun(self):  # remove this Ugen from the run set
        o2lite.send_cmd("/arco/unrun", 0, "i", self.arco_ref())

    def get(self, input_name):
        return self.inputs[input_name]


    def set(self, input_name, value, chan=0):
        previous = self.inputs.get(input_name)

        if isinstance(value, (int, float)):
            if previous.rate == C_RATE:
                if chan >= previous.chans:
                    print(f"ERROR: const {str(input_name)} of "
                          f"{self.classname} has {str(previous.chans)} "
                          f"channels but attempt to set channel {str(chan)}")
                    return
                previous.set_chan(chan, value)
                return
            value = Const(value)

        # value is a Ugen: replace the input
        elif isinstance(value, list):
            # Array of numbers: update multiple channels of a Const input.
            # If current input is not Const, replace it with a new Const.
            if previous.rate == C_RATE:
                if chan != 0:
                    print(f"ERROR: setting {input} of {classname} to an array "
                          f" of values, but non-zero chan ({chan}) specified.")
                if len(value) > previous.chans:
                    print(f"ERROR: setting {input} of {classname} to "
                          f"{value} but current Const input is mono.")
                previous.set(value)
                return
            value = Const(value)

        # value is a Ugen: replace the input
        self.inputs[input_name] = value
        addr = "/arco/" + self.classname.lower() + "/repl_" + str(input_name)
        o2lite.send_cmd(addr, 0, "ii", self.arco_ref(), value.arco_ref())


    def atend(self, action, target=None, mask=ACTION_END_OR_TERM):
        """Register an action (MUTE or FINISH) when this ugen ends."""
        if action == MUTE or action == FINISH:
            arco.register_action(self, mask, target or self, action)
        else:
            print("ERROR: Ugen.atend - unknown action", repr(action))


    def term(self, dur=0):
        """Enable termination; after ugen ends, terminate after dur seconds."""
        o2lite.send_cmd("/arco/term", 0, "if", self.arco_ref(), dur)
        return self


    def trace(self, trace_flag=True):
        """Set UGENTRACE flag for debugging."""
        o2lite.send_cmd("/arco/trace", 0, "ii", self.arco_ref(),
                        1 if trace_flag else 0)
        return self


def create_fader(input, current, dur=None, goal=None, chans=None):
    """Helper to create a Fader with optional dur/goal initialization."""
    f = Fader(input, current, chans=chans)
    if dur is not None:
        f.set_dur(dur)
    if goal is not None:
        f.set_goal(goal)
    return f


# An abstract class for Const and Smoothb
class Const_like(Ugen):

    def __init__(self, clname, values, chans, addr):
        self.values_array = values
        super().__init__(new_ugen_id(), clname, chans, C_RATE,
                       "", no_msg=True)
        self.send_floats(values, addr)

    def value():
        return values_array[0] if chans == 1 else values_array

    def send_floats(self, values, addr):
        x = []

        # Send a message with address msg by adding float values
        for i in range(self.chans):
            if isinstance(values, (int, float)):
                f = values  # same value for all chans
            else:  # values is a list
                f = values[i] if len(values) > i else 0
            x.append(f)

        o2lite.send_cmd(addr, 0, "i" + "f" * self.chans, self.arco_ref(), *x)


class Const(Const_like):

    def __init__(self, values, chans=None):
        # values is initial value or array of initial values
        # chans is length of values (chans_default) unless specified
        # by the parameter
        chans_default = max(len(values), 1) if isinstance(values, list) else 1
        super().__init__("Const", values, chans if chans else chans_default,
                         "/arco/const/newn")

    def set(self, values):
        # overload the set method of Ugen
        self.send_floats(values, "/arco/const/setn")
        return self

    def set_chan(self, chan, value):
        # Set the value of a specific channel
        if chan >= chans:
            print("ERROR: set_chan got chan", chan, "but Const has", chans,
                  "channels.")
            return self
        o2lite.send_cmd("/arco/const/set", 0, "iif", self.arco_ref(),
                        chan, value)
        return self
