from arco_engine import (
    get_engine,
    A_RATE, B_RATE, C_RATE, NO_RATE,
    ZERO_ID, ZEROB_ID, INPUT_ID, OUTPUT_ID,
    AR, BL, BR, BP,
    FADE_LINEAR, FADE_SMOOTH, FADE_EXPONENTIAL, FADE_LOWPASS,
    BLEND_LINEAR, BLEND_POWER, BLEND_45,
    MATH_OP_MUL, MATH_OP_ADD, MATH_OP_SUB, MATH_OP_DIV,
    MATH_OP_MAX, MATH_OP_MIN, MATH_OP_CLP, MATH_OP_POW,
    MATH_OP_LT, MATH_OP_GT, MATH_OP_SCP, MATH_OP_PWI,
    MATH_OP_RND, MATH_OP_SH, MATH_OP_QNT, MATH_OP_RLI,
    MATH_OP_HZDIFF, MATH_OP_TAN, MATH_OP_ATAN2, MATH_OP_SIN, MATH_OP_COS,
    UNARY_OP_ABS, UNARY_OP_NEG, UNARY_OP_EXP, UNARY_OP_LOG,
    UNARY_OP_LOG10, UNARY_OP_LOG2, UNARY_OP_SQRT,
    UNARY_OP_STEP_TO_HZ, UNARY_OP_HZ_TO_STEP,
    UNARY_OP_VEL_TO_LINEAR, UNARY_OP_LINEAR_TO_VEL,
    UNARY_OP_DB_TO_LINEAR, UNARY_OP_LINEAR_TO_DB,
    DNSAMPLE_BASIC, DNSAMPLE_AVG, DNSAMPLE_PEAK, DNSAMPLE_RMS,
    DNSAMPLE_POWER, DNSAMPLE_LOWPASS500, DNSAMPLE_LOWPASS100,
    ACTION_END_OR_TERM, ACTION_FREE, MUTE, FINISH,
    max_chans,
)

import threading

# Seconds past a fade's duration before the client drops its reference to
# a terminated Fader (server frees it at termination).
_FADE_CLEANUP_MARGIN = 0.5

import logging

log = logging.getLogger("pyarco")


class Ugen:

    def __init__(
        self,
        classname_,
        chans_,
        rate_,
        types_,
        no_msg=None,
        omit_chans=None,
        *inputs_,
        engine=None,
        id_num=None,
    ):
        self.engine = engine or get_engine()

        inputs_ = list(inputs_)
        if id_num is not None:
            self.id = id_num
            self.owns_id = False  # caller/engine manages this id's lifetime
        else:
            self.id = self.engine.id_pool.request_slot()
            self.owns_id = True
            self.engine.register(self)
        self.classname = classname_
        self.chans = chans_
        self.rate = rate_
        self._addr_prefix = "/arco/" + classname_.lower()
        self.inputs = {}
        self.action_id = None
        self._server_freed = False  # set when the server has already freed this id
        self._fade_out = None  # in-flight fade-out Fader, cleared by _drop_after

        if no_msg:
            return

        # Convert numbers to const()
        for i in range(1, len(inputs_), 2):
            if types_[i // 2] == "U" and (isinstance(inputs_[i], (int, float))
                                          or isinstance(inputs_[i], list)):
                inputs_[i] = Const(inputs_[i], None)

        # construct the message
        address = f"{self._addr_prefix}/new"
        params = []
        type_str = "i"  # only the id at first

        params.append(self.id)

        if not omit_chans:  # some Ugens do not have chans parameter
            params.append(self.chans)
            type_str += "i"

        for i in range(0, len(inputs_), 2):
            inp = inputs_[i + 1]  # the value
            if types_[i // 2] == "U":
                params.append(inp.id)
                self.inputs[
                    inputs_[i]] = inp  # put the kv pair in the inputs dict
                type_str += "i"
            else:  # one of "sihdft"
                params.append(inp)
                type_str += types_[i // 2]

        self.engine.send_cmd(address, 0, type_str, *params)
        log.debug("Ugen %d (%s) created", self.id, self.classname)

    def __del__(self):
        engine = getattr(self, 'engine', None)
        if engine is None or not getattr(self, 'owns_id', False):
            return
        if not getattr(self, '_server_freed', False):
            engine.send_cmd("/arco/free", 0, "i", self.id)
        engine.id_pool.free_slot(self.id)
        engine.unregister(self.id)

    def play(self):
        output = self.engine.output
        if output is not None:
            output.members[self.id] = self
        self.engine.send_cmd("/arco/sum/ins", 0, "ii", OUTPUT_ID, self.id)

    def mute(self, status=None):
        # status is accepted (passed by atend actions) but ignored here
        # If a fade-out is in flight, the Fader is what's in the output.
        target = self._fade_out or self
        output = self.engine.output
        if output is not None:
            output.members.pop(target.id, None)
        self.engine.send_cmd("/arco/sum/rem", 0, "ii", OUTPUT_ID, target.id)

    def fade(self, dur, mode=FADE_SMOOTH):
        """Fade output to zero over dur seconds, then disconnect."""
        fader = self.engine.fade_in_lookup.pop(self.id, None)
        if fader is not None:
            # fade_in is in progress; convert to fade out
            fader.set_dur(dur)
            fader.set_goal(0)
            fader.set_mode(mode)
            self._fade_out = fader
            self._drop_after(fader, dur)
            return fader
        if self._fade_out is not None:
            # a fade-out is already in flight; adjust it, don't re-swap
            fader = self._fade_out
            fader.set_dur(dur)
            fader.set_mode(mode)
            return fader
        faded = create_fader(self, 1, dur, 0)
        faded.term()
        output = self.engine.output
        if output is not None:
            output.members.pop(self.id, None)
            output.members[faded.id] = faded
        # swap self out of output, put faded in
        self.engine.send_cmd("/arco/sum/swap", 0, "iii", OUTPUT_ID,
                             self.id, faded.id)
        faded.set_mode(mode)
        self._fade_out = faded
        self._drop_after(faded, dur)
        return faded

    def _drop_after(self, fader, dur):
        """After a terminating fade completes, the server frees the Fader;
        drop the client-side reference so its slot can be reclaimed."""
        engine = self.engine

        def _cleanup():
            fader._server_freed = True
            if self._fade_out is fader:
                self._fade_out = None
            output = engine.output
            if output is not None:
                output.members.pop(fader.id, None)

        threading.Timer(dur + _FADE_CLEANUP_MARGIN, _cleanup).start()

    def fade_in(self, dur, mode=FADE_SMOOTH, term=True):
        """Fade in from silence. Ugen must NOT already be playing."""
        fader = create_fader(self, 0, dur, 1)
        if term:
            fader.term()
        self.engine.fade_in_lookup[self.id] = fader
        fader.set_mode(mode)
        fader.play()
        engine = self.engine

        def _fade_in_complete():
            f = engine.fade_in_lookup.pop(self.id, None)
            if f is not None:
                # swap fader out, put the original ugen directly in output
                engine.send_cmd("/arco/sum/swap", 0, "iii", OUTPUT_ID,
                                f.id, self.id)
                f._server_freed = True
                output = engine.output
                if output is not None:
                    output.members.pop(f.id, None)
                    output.members[self.id] = self

        threading.Timer(dur + _FADE_CLEANUP_MARGIN,
                        _fade_in_complete).start()

    def run(self):  # add this Ugen to the run set
        self.engine.send_cmd("/arco/run", 0, "i", self.id)

    def unrun(self):  # remove this Ugen from the run set
        self.engine.send_cmd("/arco/unrun", 0, "i", self.id)

    def get(self, input_name):
        return self.inputs[input_name]

    def set(self, input_name, value, chan=0):
        previous = self.inputs.get(input_name)
        if previous is None:
            if not isinstance(input_name, str):
                print("ERROR: set() called with input " + str(input_name) +
                      " of type '" + type(input_name).__name__ + "'")
            else:
                print("ERROR: " + str(input_name) + " not found in '" +
                      self.classname + "'")
            return

        addr_prefix = self._addr_prefix

        if isinstance(value, list):
            # Array of numbers: update multiple channels of a Const input.
            # If current input is not Const, replace it with a new Const.
            if previous.rate != C_RATE:
                value = Const(value, None)
                self.inputs[input_name] = value
                self.engine.send_cmd(addr_prefix + "/repl_" + str(input_name),
                                0, "ii", self.id, value.id)
            else:
                for i, v in enumerate(value):
                    if i < previous.chans:
                        self.engine.send_cmd(addr_prefix + "/set_" +
                                        str(input_name),
                                        0, "iif", self.id, i, v)
            return

        if isinstance(value, (int, float)):
            if previous.rate == C_RATE:
                if chan >= previous.chans:
                    print("ERROR: const '" + str(input_name) + "' of '" +
                          self.classname + "' has " + str(previous.chans) +
                          " channels but attempt to set channel " + str(chan))
                    return
                self.engine.send_cmd(addr_prefix + "/set_" + str(input_name),
                                0, "iif", self.id, chan, value)
                return
            value = Const(value, None)

        # value is a Ugen: replace the input
        self.inputs[input_name] = value
        self.engine.send_cmd(addr_prefix + "/repl_" + str(input_name),
                        0, "ii", self.id, value.id)

    def atend(self, action, target=None, mask=ACTION_END_OR_TERM):
        """Register an action (MUTE or FINISH) when this ugen ends."""
        if action == MUTE or action == FINISH:
            self.engine.register_action(self, mask, target or self, action)
        else:
            print("ERROR: Ugen.atend - unknown action", repr(action))

    def term(self, dur=0):
        """Enable termination; after ugen ends, terminate after dur seconds."""
        self.engine.send_cmd("/arco/term", 0, "if", self.id, dur)
        return self

    def trace(self, trace_flag=True):
        """Set UGENTRACE flag for debugging."""
        self.engine.send_cmd("/arco/trace", 0, "ii", self.id,
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

    def send_floats(self, values, msg):
        x = []

        # Send a message with address msg by adding float values
        for i in range(self.chans):
            if isinstance(values, (int, float)):
                f = values  # same value for all chans
            else:  # values is a list
                f = values[i] if len(values) > i else 0
            x.append(f)

        self.engine.send_cmd(msg, 0, "i" + "f" * self.chans, self.id, *x)


class Const(Const_like):

    def __init__(self, values, chans=None):
        # values is initial value or array of initial values
        # chans is length of values (chans_default) unless specified by the parameter
        chans_default = max(len(values), 1) if isinstance(values, list) else 1
        self.chans = chans if chans else chans_default
        super().__init__(
            "Const", self.chans, C_RATE, "", True, None
        )  # no_msg=True to issue a special message and ignore the inputs
        self.send_floats(values, "/arco/const/newn")

    def set(self, values):
        # overload the set method of Ugen
        self.send_floats(values, "/arco/const/setn")
        return self

    def set_chan(self, chan, value):
        # Set the value of a specific channel
        self.engine.send_cmd("/arco/const/set", 0, "iif", self.id, chan, value)
        return self


class Sine(Ugen):

    def __init__(self, freq, amp, chans=None):
        if chans is None:
            chans = max_chans(max_chans(1, freq), amp)
        super().__init__("Sine", chans, A_RATE, "UU", None, None, "freq", freq,
                         "amp", amp)


class Sineb(Ugen):

    def __init__(self, freq, amp, chans=None):
        if not isinstance(freq, (int, float)) and freq.rate != B_RATE:
            print("ERROR: 'freq' input to Ugen 'sineb' must be block rate")
            return
        if not isinstance(amp, (int, float)) and amp.rate != B_RATE:
            print("ERROR: 'amp' input to Ugen 'sineb' must be block rate")
            return

        if chans is None:
            chans = max_chans(max_chans(1, freq), amp)

        super().__init__("Sineb", chans, B_RATE, "UU", None, None, "freq",
                         freq, "amp", amp)


class Fader(Ugen):

    def __init__(self, input, current, mode=FADE_SMOOTH, chans=None):
        if chans is None:
            chans = max_chans(1, input)
        super().__init__("Fader", chans, A_RATE, "Ufi", None, None, 'input',
                         input, 'current', current, 'mode', mode)

    def set_current(self, current, chan=None):
        if chan is not None:
            self.engine.send_cmd("/arco/fader/cur", 0, "iif", self.id, chan,
                            current)
        else:
            for i in range(self.chans):
                self.engine.send_cmd(
                    "/arco/fader/cur", 0, "iif", self.id, i,
                    current if isinstance(current,
                                          (int, float)) else current[i])
        return self

    def set_dur(self, dur):
        self.engine.send_cmd("/arco/fader/dur", 0, "if", self.id, dur)
        return self

    def set_mode(self, mode):
        self.engine.send_cmd("/arco/fader/mode", 0, "ii", self.id, mode)
        return self

    def set_goal(self, goal, chan=None):
        if chan is not None:
            self.engine.send_cmd("/arco/fader/goal", 0, "iif", self.id, chan, goal)
        else:
            for i in range(self.chans):
                self.engine.send_cmd(
                    "/arco/fader/goal", 0, "iif", self.id, i,
                    goal if isinstance(goal, (int, float)) else goal[i])
        return self


class Smoothb(Const_like):

    def __init__(self, x, cutoff=10, chans=None):
        chans_default = max(len(x), 1) if isinstance(x, list) else 1
        self.chans = chans if chans else chans_default

        super().__init__("Smoothb", self.chans, B_RATE, "", True, None)

        values = x if isinstance(x, list) else [x] * self.chans
        floats = [cutoff] + [values[i] if i < len(values) else 0
                             for i in range(self.chans)]
        self.engine.send_cmd("/arco/smoothb/newn", 0,
                             "i" + "f" * len(floats), self.id, *floats)

    def set(self, x):
        self.send_floats(x, "/arco/smoothb/setn")
        return self

    def set_chan(self, chan, x):
        self.engine.send_cmd("/arco/smoothb/set", 0, "iif", self.id, chan, x)
        return self

    def set_cutoff(self, cutoff):
        self.engine.send_cmd("/arco/smoothb/cutoff", 0, "if", self.id, cutoff)
        return self


class Delay(Ugen):

    def __init__(self, input, dur, fb, maxdur, chans=None):
        if chans is None:
            chans = max_chans(1, input)
        super().__init__("Delay", chans, A_RATE, "UUUf", None, None, 'input',
                         input, 'dur', dur, 'fb', fb, 'maxdur', maxdur)


class Feedback(Ugen):

    def __init__(self, input, from_ugen, gain, chans=1):
        if chans is None:
            chans = max_chans(max_chans(max_chans(1, input), from_ugen), gain)
        super().__init__("Feedback", chans, A_RATE, "UUU", None, None, 'input',
                         input, 'from', from_ugen, 'gain', gain)


class Reson(Ugen):

    def __init__(self, snd, center, q, chans=None):
        if not isinstance(snd, (int, float)) and snd.rate != A_RATE:
            print("ERROR: 'snd' input to Ugen 'reson' must be audio rate")
            return None
        if chans is None:
            chans = max_chans(max_chans(max_chans(1, snd), center), q)
        super().__init__("Reson", chans, A_RATE, "UUU", None, None, 'snd', snd,
                         'center', center, 'q', q)


class Resonb(Ugen):

    def __init__(self, snd, center, q, chans=None):
        if not isinstance(snd, (int, float)) and snd.rate != B_RATE:
            print("ERROR: 'snd' input to Ugen 'resonb' must be block rate")
            return None
        if not isinstance(center, (int, float)) and center.rate != B_RATE:
            print("ERROR: 'center' input to Ugen 'resonb' must be block rate")
            return None
        if not isinstance(q, (int, float)) and q.rate != B_RATE:
            print("ERROR: 'q' input to Ugen 'resonb' must be block rate")
            return None
        if chans is None:
            chans = max_chans(max_chans(max_chans(1, snd), center), q)
        super().__init__("Resonb", chans, B_RATE, "UUU", None, None, 'snd',
                         snd, 'center', center, 'q', q)


class Lowpass(Ugen):

    def __init__(self, snd, cutoff, chans=None):
        if not isinstance(snd, (int, float)) and snd.rate != A_RATE:
            print("ERROR: 'snd' input to Ugen 'lowpass' must be audio rate")
            return None
        if chans is None:
            chans = max_chans(max_chans(1, snd), cutoff)
        super().__init__("Lowpass", chans, A_RATE, "UU", None, None, 'snd',
                         snd, 'cutoff', cutoff)


class Allpass(Ugen):

    def __init__(self, input, dur, fb, maxdur, chans=None):
        if chans is None:
            chans = max_chans(1, input)
        super().__init__("Allpass", chans, A_RATE, "UUUf", None, None, 'input',
                         input, 'dur', dur, 'fb', fb, 'maxdur', maxdur)


class Blend(Ugen):

    def __init__(self, x1, x2, b, mode=BLEND_LINEAR, init_b=0.5, chans=None,
                 gain=1):
        if chans is None:
            chans = max_chans(max_chans(max_chans(1, x1), x2), b)
        super().__init__("Blend", chans, A_RATE, "UUUfi", None, None, 'x1', x1,
                         'x2', x2, 'b', b, 'init_b', init_b, 'mode', mode)
        if gain != 1:
            self.set_gain(gain)

    def set_gain(self, gain):
        self.engine.send_cmd("/arco/blend/gain", 0, "if", self.id, gain)
        return self

    def set_mode(self, mode):
        self.engine.send_cmd("/arco/blend/mode", 0, "ii", self.id, mode)
        return self


class Blendb(Ugen):

    def __init__(self, x1, x2, b, mode=BLEND_LINEAR, chans=None, gain=1):
        if chans is None:
            chans = max_chans(max_chans(max_chans(1, x1), x2), b)
        super().__init__("Blendb", chans, B_RATE, "UUUi", None, None, 'x1', x1,
                         'x2', x2, 'b', b, 'mode', mode)
        if gain != 1:
            self.set_gain(gain)

    def set_gain(self, gain):
        self.engine.send_cmd("/arco/blendb/gain", 0, "if", self.id, gain)
        return self

    def set_mode(self, mode):
        self.engine.send_cmd("/arco/blendb/mode", 0, "ii", self.id, mode)
        return self


class Fileplay(Ugen):

    def __init__(self,
                 filename,
                 chans=2,
                 start=0,
                 end=0,
                 cycle=False,
                 mix=False,
                 expand=False):
        super().__init__("Fileplay", chans, A_RATE, "sffBBB", None, None,
                         'filename', filename, 'start', start, 'end', end,
                         'cycle', cycle, 'mix', mix, 'expand', expand)

    def go(self, play_flag=True):
        self.engine.send_cmd("/arco/fileplay/play", 0, "iB", self.id, play_flag)
        return self

    def stop(self):
        return self.go(False)


class Filerec(Ugen):

    def __init__(self, filename, input, chans=2):
        super().__init__("Filerec", chans, '', "sU", None, None, 'filename',
                         filename, 'input', input)

    def go(self, rec_flag=True):
        self.engine.send_cmd("/arco/filerec/rec", 0, "iB", self.id, rec_flag)
        return self

    def stop(self):
        return self.go(False)


class Recplay(Ugen):

    def __init__(self, input, chans=1, gain=1, fade_time=0.1, loop=False):
        super().__init__("Recplay", chans, A_RATE, "UUfB", None, None, 'input',
                         input, 'gain', gain, 'fade_time', fade_time, 'loop',
                         loop)

    def record(self, record_flag):
        self.engine.send_cmd("/arco/recplay/rec", 0, "iB", self.id, record_flag)
        return self

    def start(self, start_time):
        self.engine.send_cmd("/arco/recplay/start", 0, "id", self.id, start_time)
        return self

    def stop(self):
        self.engine.send_cmd("/arco/recplay/stop", 0, "i", self.id)
        return self

    def set_speed(self, x):
        self.engine.send_cmd("/arco/recplay/speed", 0, "if", self.id, x)
        return self

    def borrow(self, u):
        self._lender = u  # pin: server borrows u's buffer; keep it alive
        self.engine.send_cmd("/arco/recplay/borrow", 0, "ii", self.id, u.id)
        return self


class Sum(Ugen):

    def __init__(self, chans, wrap=True, id_num=None):
        super().__init__("Sum", chans, A_RATE, "i", None, None, 'wrap',
                         1 if wrap else 0, id_num=id_num)
        self.members = {}  # id -> Ugen: mirror server graph client-side

    def ins(self, *ugens):
        for ugen in ugens:
            self.members[ugen.id] = ugen
            self.engine.send_cmd("/arco/sum/ins", 0, "ii", self.id, ugen.id)
        return self

    def rem(self, *ugens):
        for ugen in ugens:
            self.members.pop(ugen.id, None)
            self.engine.send_cmd("/arco/sum/rem", 0, "ii", self.id, ugen.id)
        return self

    def swap(self, ugen, replacement):
        self.members.pop(ugen.id, None)
        self.members[replacement.id] = replacement
        self.engine.send_cmd("/arco/sum/swap", 0, "iii", self.id, ugen.id,
                             replacement.id)
        return self

    def set_gain(self, gain):
        self.engine.send_cmd("/arco/sum/set_gain", 0, "if", self.id, gain)
        return self


class Sumb(Ugen):

    def __init__(self, chans, wrap=True, id_num=None):
        super().__init__("Sumb", chans, B_RATE, "i", None, None, 'wrap',
                         1 if wrap else 0, id_num=id_num)
        self.members = {}  # id -> Ugen: mirror server graph client-side

    def ins(self, *ugens):
        for ugen in ugens:
            self.members[ugen.id] = ugen
            self.engine.send_cmd("/arco/sumb/ins", 0, "ii", self.id, ugen.id)
        return self

    def rem(self, *ugens):
        for ugen in ugens:
            self.members.pop(ugen.id, None)
            self.engine.send_cmd("/arco/sumb/rem", 0, "ii", self.id, ugen.id)
        return self

    def swap(self, ugen, replacement):
        self.members.pop(ugen.id, None)
        self.members[replacement.id] = replacement
        self.engine.send_cmd("/arco/sumb/swap", 0, "iii", self.id, ugen.id,
                             replacement.id)
        return self

    def set_gain(self, gain):
        self.engine.send_cmd("/arco/sumb/set_gain", 0, "if", self.id, gain)
        return self


class Route(Ugen):

    def __init__(self, chans):
        super().__init__("Route", chans, A_RATE, "", None, None)
        self.members = {}  # id -> Ugen; released by reminput()

    def ins(self, input, *routes):
        self.members[input.id] = input
        self._send_ins_rem(input, routes, "/arco/route/ins")
        return self

    def rem(self, input, *routes):
        self._send_ins_rem(input, routes, "/arco/route/rem")
        return self

    def reminput(self, input):
        self.members.pop(input.id, None)
        self.engine.send_cmd("/arco/route/reminput", 0, "ii", self.id,
                             input.id)
        return self

    def _send_ins_rem(self, input, routes, address):
        params = [self.id, input.id]
        params.extend(routes)
        type_str = "i" * (len(params))

        self.engine.send_cmd(address, 0, type_str, *params)


class Vu(Ugen):

    def __init__(self, reply_addr, period):
        super().__init__("Vu", 0, '', "sf", None, None, 'reply_addr',
                         reply_addr, 'period', period)

    def start(self, reply_addr, period):
        self.engine.send_cmd("/arco/vu/start", 0, "isf", self.id, reply_addr,
                        period)
        return self

    def set(self, input_name, value):
        self.inputs['input'] = value
        log.debug("Vu set %d %d", self.id, value.id)
        self.engine.send_cmd("/arco/vu/repl_input", 0, "ii", self.id, value.id)
        return self


class Trig(Ugen):

    def __init__(self, input, reply_addr, window, threshold, pause):
        super().__init__("Trig", 0, A_RATE, "Usiff", None, True, 'input',
                         input, 'reply_addr', reply_addr, 'window', window,
                         'threshold', threshold, 'pause', pause)

    def set_window(self, window):
        self.engine.send_cmd("/arco/trig/window", 0, "ii", self.id, window)
        return self

    def set_threshold(self, threshold):
        self.engine.send_cmd("/arco/trig/threshold", 0, "if", self.id, threshold)
        return self

    def set_pause(self, pause):
        self.engine.send_cmd("/arco/trig/pause", 0, "if", self.id, pause)
        return self

    def onoff(self, reply_addr, threshold, runlen):
        self.engine.send_cmd("/arco/trig/onoff", 0, "isff", self.id, reply_addr,
                        threshold, runlen)
        return self


class Thru(Ugen):

    def __init__(self, input, chans=1, id_num=None):
        super().__init__("Thru", chans, A_RATE, "U", None, None, 'input',
                         input, id_num=id_num)

    def set_alternate(self, alt):
        self._alternate = alt  # pin: server's alt pointer must not outlive our ref
        self.engine.send_cmd("/arco/thru/alt", 0, "ii", self.id, alt.id)
        return self


class Fanout(Thru):

    def __init__(self, input, chans):
        super().__init__(input, chans)


class Dnsampleb(Ugen):

    def __init__(self, input, mode, chans=1):
        super().__init__("Dnsampleb", chans, B_RATE, "Ui", None, None, 'input',
                         input, 'mode', mode)

    def set_cutoff(self, hz):
        self.engine.send_cmd("/arco/dnsampleb/cutoff", 0, "if", self.id, hz)
        return self

    def set_mode(self, mode):  # refer to the constants, like 'DNSAMPLE_BASIC'
        self.engine.send_cmd("/arco/dnsampleb/mode", 0, "ii", self.id, mode)
        return self


class Dualslewb(Ugen):

    def __init__(self,
                 input,
                 chans=1,
                 attack=0.02,
                 release=0.02,
                 current=0,
                 attack_linear=True,
                 release_linear=True):
        super().__init__("Dualslewb", chans, B_RATE, "Ufffii", None, None,
                         'input', input, 'attack', attack, 'release', release,
                         'current', current, 'attack_linear',
                         1 if attack_linear else 0, 'release_linear',
                         1 if release_linear else 0)

    def set_current(self, current):
        self.engine.send_cmd("/arco/dualslewb/current", 0, "if", self.id, current)
        return self

    def set_attack(self, attack, attack_linear=True):
        self.engine.send_cmd("/arco/dualslewb/attack", 0, "ifi", self.id, attack,
                        1 if attack_linear else 0)
        return self

    def set_release(self, release, release_linear=True):
        self.engine.send_cmd("/arco/dualslewb/release", 0, "ifi", self.id, release,
                        1 if release_linear else 0)
        return self


class Probe(Ugen):

    def __init__(self, input, reply_addr):
        self.running = False
        super().__init__("Probe", 0, NO_RATE, "Us", None, True, 'input', input,
                         'reply_addr', reply_addr)

    def probe(self, period, frames, chan, nchans, stride, repeats=0):
        self.engine.send_cmd("/arco/probe/probe", 0, "ifiiiii", self.id, period,
                        frames, chan, nchans, stride, repeats)
        if not self.running:
            self.run()  # suppress warning if we're already in run set
            self.running = True
        return self

    def thresh(self, threshold, direction, max_wait):
        self.engine.send_cmd("/arco/probe/thresh", 0, "ifif", self.id, threshold,
                        direction, max_wait)
        return self

    def stop(self):
        self.engine.send_cmd("/arco/probe/stop", 0, "i", self.id)
        if self.running:
            self.unrun()
            self.running = False
        return self


class Envelope(Ugen):

    def __init__(self,
                 classname,
                 addr,
                 rate,
                 points,
                 init=None,
                 start=False,
                 lin=None):
        self.address = addr
        super().__init__(classname, 1, rate, "", None, True)
        self.sample_rate = AR if rate == A_RATE else BR
        self.set_point_array(points)

        if init is not None:
            self.set(init)
        if start:
            self.start()
        if lin is not None:
            self.linear_attack(lin)

    def set_point_array(self, points):
        params = [self.id]
        time = 0
        count = 0

        for i in range(0, len(points), 2):
            time += points[i]
            samples = max(1, round(time * self.sample_rate - count))
            count += samples
            params.append(float(samples))

            if i + 1 < len(
                    points):  # if length is odd, append a zero amplitude
                params.append(float(points[i + 1]))

        type_str = "i" + "f" * (len(params) - 1)
        self.engine.send_cmd(f"{self.address}env", 0, type_str, *params)
        return self

    def set_points(self, *points):
        self.set_point_array(points)
        return self

    def start(self):
        self.engine.send_cmd(f"{self.address}start", 0, "i", self.id)
        return self

    def stop(self):
        self.engine.send_cmd(f"{self.address}stop", 0, "i", self.id)
        return self

    def decay(self, dur):
        self.engine.send_cmd(f"{self.address}decay", 0, "if", self.id,
                        dur * self.sample_rate)
        return self

    def linear_attack(self, lin=True):
        self.engine.send_cmd(f"{self.address}linatk", 0, "iB", self.id, lin)
        return self

    def set(self, y):
        self.engine.send_cmd(f"{self.address}set", 0, "if", self.id, y)
        return self


class Pwl(Envelope):

    def __init__(self, points, initial_value=None, start=True):
        super().__init__("Pwl", "/arco/pwl/", A_RATE, points, initial_value,
                         start)


class Pwlb(Envelope):

    def __init__(self, points, initial_value=None, start=True):
        super().__init__("Pwlb", "/arco/pwlb/", B_RATE, points, initial_value,
                         start)


class Pwe(Envelope):

    def __init__(self, points, initial_value=None, start=True, lin=None):
        super().__init__("Pwe", "/arco/pwe/", A_RATE, points, initial_value,
                         start, lin)


class Pweb(Envelope):

    def __init__(self, points, initial_value=None, start=True, lin=None):
        super().__init__("Pweb", "/arco/pweb/", B_RATE, points, initial_value,
                         start, lin)


class Pv(Ugen):

    def __init__(self, input, ratio, fftsize, hopsize, points, mode, chans=1):
        super().__init__("Pv", chans, A_RATE, "Ufiiii", None, None, 'input',
                         input, 'ratio', ratio, 'fftsize', fftsize, 'hopsize',
                         hopsize, 'points', points, 'mode', mode)

    def set_ratio(self, value):
        self.engine.send_cmd("/arco/pv/ratio", 0, "if", self.id, value)
        return self

    def set_stretch(self, value):
        self.engine.send_cmd("/arco/pv/stretch", 0, "if", self.id, value)
        return self


class Granstream(Ugen):

    def __init__(self, input, polyphony, dur, enable, chans=1):
        super().__init__("Granstream", chans, A_RATE, "UifB", None, None,
                         'input', input, 'polyphony', polyphony, 'dur', dur,
                         'enable', enable)

    def set_gain(self, gain):
        self.engine.send_cmd("/arco/granstream/gain", 0, "if", self.id, gain)
        return self

    def set_polyphony(self, p):
        self.engine.send_cmd("/arco/granstream/polyphony", 0, "if", self.id, p)
        return self

    def set_ratio(self, low, high):
        self.engine.send_cmd("/arco/granstream/ratio", 0, "iff", self.id, low, high)
        return self

    def set_graindur(self, lowdur, highdur):
        self.engine.send_cmd("/arco/granstream/graindur", 0, "iff", self.id, lowdur,
                        highdur)
        return self

    def set_density(self, density):
        self.engine.send_cmd("/arco/granstream/density", 0, "if", self.id, density)
        return self

    def set_env(self, attack, release):
        self.engine.send_cmd("/arco/granstream/env", 0, "iff", self.id, attack,
                        release)
        return self

    def set_enable(self, enable):
        self.engine.send_cmd("/arco/granstream/enable", 0, "iB", self.id, enable)
        return self

    def set_dur(self, dur):
        self.engine.send_cmd("/arco/granstream/dur", 0, "if", self.id, dur)
        return self

    def set_delay(self, d):
        self.engine.send_cmd("/arco/granstream/delay", 0, "if", self.id, d)
        return self

    def set_feedback(self, fb):
        self.engine.send_cmd("/arco/granstream/feedback", 0, "if", self.id, fb)
        return self


class Math(Ugen):

    def __init__(self, op, x1, x2, chans=None):
        if not chans:
            chans = max_chans(max_chans(1, x1), x2)
        super().__init__("Math", chans, A_RATE, "iUU", None, None, 'op', op,
                         'x1', x1, 'x2', x2)

    def rliset(self, x):
        self.engine.send_cmd("/arco/math/rliset", 0, "if", self.id, x)
        return self

    @staticmethod
    def add(x1, x2, chans=None):
        return Math(MATH_OP_ADD, x1, x2, chans)

    @staticmethod
    def sub(x1, x2, chans=None):
        return Math(MATH_OP_SUB, x1, x2, chans)

    @staticmethod
    def mult(x1, x2, chans=None, x2_init=None):
        if x2_init is not None:
            return Multx(x1, x2, x2_init, chans)
        else:
            return Math(MATH_OP_MUL, x1, x2, chans)

    @staticmethod
    def div(x1, x2, chans=None):
        return Math(MATH_OP_DIV, x1, x2, chans)

    @staticmethod
    def max(x1, x2, chans=None):
        return Math(MATH_OP_MAX, x1, x2, chans)

    @staticmethod
    def min(x1, x2, chans=None):
        return Math(MATH_OP_MIN, x1, x2, chans)

    @staticmethod
    def clip(x1, x2, chans=None):
        return Math(MATH_OP_CLP, x1, x2, chans)

    @staticmethod
    def pow(x1, x2, chans=None):
        return Math(MATH_OP_POW, x1, x2, chans)

    @staticmethod
    def less(x1, x2, chans=None):
        return Math(MATH_OP_LT, x1, x2, chans)

    @staticmethod
    def greater(x1, x2, chans=None):
        return Math(MATH_OP_GT, x1, x2, chans)

    @staticmethod
    def soft_clip(x1, x2, chans=None):
        return Math(MATH_OP_SCP, x1, x2, chans)

    @staticmethod
    def powi(x1, x2, chans=None):
        return Math(MATH_OP_PWI, x1, x2, chans)

    @staticmethod
    def rand(x1, x2, chans=None):
        return Math(MATH_OP_RND, x1, x2, chans)

    @staticmethod
    def sample_hold(x1, x2, chans=None):
        return Math(MATH_OP_SH, x1, x2, chans)

    @staticmethod
    def quantize(x1, x2, chans=None):
        return Math(MATH_OP_QNT, x1, x2, chans)

    @staticmethod
    def rli(x1, x2, chans=None):
        return Math(MATH_OP_RLI, x1, x2, chans)

    @staticmethod
    def hzdiff(x1, x2, chans=None):
        return Math(MATH_OP_HZDIFF, x1, x2, chans)

    @staticmethod
    def tan(x1, x2, chans=None):
        return Math(MATH_OP_TAN, x1, x2, chans)

    @staticmethod
    def atan2(x1, x2, chans=None):
        return Math(MATH_OP_ATAN2, x1, x2, chans)

    @staticmethod
    def sin(x1, x2, chans=None):
        return Math(MATH_OP_SIN, x1, x2, chans)

    @staticmethod
    def cos(x1, x2, chans=None):
        return Math(MATH_OP_COS, x1, x2, chans)


class Mathb(Ugen):

    def __init__(self, op, x1, x2, chans=None):
        if not isinstance(x1, (int, float)) and x1.rate != B_RATE:
            print("ERROR: 'x1' input to Ugen 'mathb' must be block rate", op,
                  x1)
            return
        elif not isinstance(x2, (int, float)) and x2.rate != B_RATE:
            print("ERROR: 'x2' input to Ugen 'mathb' must be block rate", op,
                  x2)
            return
        else:
            chans = chans or max_chans(max_chans(1, x1), x2)
            super().__init__("Mathb", chans, B_RATE, "iUU", None, None, 'op',
                             op, 'x1', x1, 'x2', x2)

    def rliset(self, x):
        self.engine.send_cmd("/arco/mathb/rliset", 0, "if", self.id, x)
        return self

    @staticmethod
    def add(x1, x2, chans=None):
        return Mathb(MATH_OP_ADD, x1, x2, chans)

    @staticmethod
    def sub(x1, x2, chans=None):
        return Mathb(MATH_OP_SUB, x1, x2, chans)

    @staticmethod
    def mult(x1, x2, chans=None):
        return Mathb(MATH_OP_MUL, x1, x2, chans)

    @staticmethod
    def div(x1, x2, chans=None):
        return Mathb(MATH_OP_DIV, x1, x2, chans)

    @staticmethod
    def max(x1, x2, chans=None):
        return Mathb(MATH_OP_MAX, x1, x2, chans)

    @staticmethod
    def min(x1, x2, chans=None):
        return Mathb(MATH_OP_MIN, x1, x2, chans)

    @staticmethod
    def clip(x1, x2, chans=None):
        return Mathb(MATH_OP_CLP, x1, x2, chans)

    @staticmethod
    def pow(x1, x2, chans=None):
        return Mathb(MATH_OP_POW, x1, x2, chans)

    @staticmethod
    def less(x1, x2, chans=None):
        return Mathb(MATH_OP_LT, x1, x2, chans)

    @staticmethod
    def greater(x1, x2, chans=None):
        return Mathb(MATH_OP_GT, x1, x2, chans)

    @staticmethod
    def soft_clip(x1, x2, chans=None):
        return Mathb(MATH_OP_SCP, x1, x2, chans)

    @staticmethod
    def powi(x1, x2, chans=None):
        return Mathb(MATH_OP_PWI, x1, x2, chans)

    @staticmethod
    def rand(x1, x2, chans=None):
        return Mathb(MATH_OP_RND, x1, x2, chans)

    @staticmethod
    def sample_hold(x1, x2, chans=None):
        return Mathb(MATH_OP_SH, x1, x2, chans)

    @staticmethod
    def quantize(x1, x2, chans=None):
        return Mathb(MATH_OP_QNT, x1, x2, chans)

    @staticmethod
    def rli(x1, x2, chans=None):
        return Mathb(MATH_OP_RLI, x1, x2, chans)

    @staticmethod
    def hzdiff(x1, x2, chans=None):
        return Mathb(MATH_OP_HZDIFF, x1, x2, chans)

    @staticmethod
    def tan(x1, x2, chans=None):
        return Mathb(MATH_OP_TAN, x1, x2, chans)

    @staticmethod
    def atan2(x1, x2, chans=None):
        return Mathb(MATH_OP_ATAN2, x1, x2, chans)

    @staticmethod
    def sin(x1, x2, chans=None):
        return Mathb(MATH_OP_SIN, x1, x2, chans)

    @staticmethod
    def cos(x1, x2, chans=None):
        return Mathb(MATH_OP_COS, x1, x2, chans)


class Multx(Ugen):

    def __init__(self, x1, x2, x2_init, chans=None):
        if not chans:
            chans = max_chans(max_chans(1, x1), x2)
        super().__init__("Multx", chans, A_RATE, "UUf", None, None, 'x1', x1,
                         'x2', x2, 'init', x2_init)


class Unary(Ugen):

    def __init__(self, op, x1, chans=None):
        if not chans:
            chans = max_chans(1, x1)
        super().__init__("Unary", chans, A_RATE, "iU", None, None, 'op', op,
                         'x1', x1)

    @staticmethod
    def abs(x1, chans=None):
        return Unary(UNARY_OP_ABS, x1, chans)

    @staticmethod
    def neg(x1, chans=None):
        return Unary(UNARY_OP_NEG, x1, chans)

    @staticmethod
    def exp(x1, chans=None):
        return Unary(UNARY_OP_EXP, x1, chans)

    @staticmethod
    def log(x1, chans=None):
        return Unary(UNARY_OP_LOG, x1, chans)

    @staticmethod
    def log10(x1, chans=None):
        return Unary(UNARY_OP_LOG10, x1, chans)

    @staticmethod
    def log2(x1, chans=None):
        return Unary(UNARY_OP_LOG2, x1, chans)

    @staticmethod
    def sqrt(x1, chans=None):
        return Unary(UNARY_OP_SQRT, x1, chans)

    @staticmethod
    def step_to_hz(x1, chans=None):
        return Unary(UNARY_OP_STEP_TO_HZ, x1, chans)

    @staticmethod
    def hz_to_step(x1, chans=None):
        return Unary(UNARY_OP_HZ_TO_STEP, x1, chans)

    @staticmethod
    def vel_to_linear(x1, chans=None):
        return Unary(UNARY_OP_VEL_TO_LINEAR, x1, chans)

    @staticmethod
    def linear_to_vel(x1, chans=None):
        return Unary(UNARY_OP_LINEAR_TO_VEL, x1, chans)

    @staticmethod
    def db_to_linear(x1, chans=None):
        return Unary(UNARY_OP_DB_TO_LINEAR, x1, chans)

    @staticmethod
    def linear_to_db(x1, chans=None):
        return Unary(UNARY_OP_LINEAR_TO_DB, x1, chans)


class Unaryb(Ugen):

    def __init__(self, op, x1, chans=None):
        if not isinstance(x1, (int, float)) and x1.rate != B_RATE:
            print("ERROR: 'x1' input to Ugen 'unaryb' must be block rate", op)
            return
        if not chans:
            chans = max_chans(1, x1)
        super().__init__("Unaryb", chans, B_RATE, "iU", None, None, 'op', op,
                         'x1', x1)

    @staticmethod
    def abs(x1, chans=None):
        return Unaryb(UNARY_OP_ABS, x1, chans)

    @staticmethod
    def neg(x1, chans=None):
        return Unaryb(UNARY_OP_NEG, x1, chans)

    @staticmethod
    def exp(x1, chans=None):
        return Unaryb(UNARY_OP_EXP, x1, chans)

    @staticmethod
    def log(x1, chans=None):
        return Unaryb(UNARY_OP_LOG, x1, chans)

    @staticmethod
    def log10(x1, chans=None):
        return Unaryb(UNARY_OP_LOG10, x1, chans)

    @staticmethod
    def log2(x1, chans=None):
        return Unaryb(UNARY_OP_LOG2, x1, chans)

    @staticmethod
    def sqrt(x1, chans=None):
        return Unaryb(UNARY_OP_SQRT, x1, chans)

    @staticmethod
    def step_to_hz(x1, chans=None):
        return Unaryb(UNARY_OP_STEP_TO_HZ, x1, chans)

    @staticmethod
    def hz_to_step(x1, chans=None):
        return Unaryb(UNARY_OP_HZ_TO_STEP, x1, chans)

    @staticmethod
    def vel_to_linear(x1, chans=None):
        return Unaryb(UNARY_OP_VEL_TO_LINEAR, x1, chans)

    @staticmethod
    def linear_to_vel(x1, chans=None):
        return Unaryb(UNARY_OP_LINEAR_TO_VEL, x1, chans)

    @staticmethod
    def db_to_linear(x1, chans=None):
        return Unaryb(UNARY_OP_DB_TO_LINEAR, x1, chans)

    @staticmethod
    def linear_to_db(x1, chans=None):
        return Unaryb(UNARY_OP_LINEAR_TO_DB, x1, chans)


class Ola_pitch_shift(Ugen):

    def __init__(self, input, ratio, xfade, windur, chans=1):
        super().__init__("Olaps", chans, A_RATE, "Ufff", None, None, 'input',
                         input, 'ratio', ratio, 'xfade', xfade, 'windur',
                         windur)

    def set_ratio(self, value):
        self.engine.send_cmd("/arco/olaps/ratio", 0, "if", self.id, value)
        return self

    def set_xfade(self, xfade):
        self.engine.send_cmd("/arco/olaps/xfade", 0, "if", self.id, xfade)
        return self

    def set_windur(self, windur):
        self.engine.send_cmd("/arco/olaps/windur", 0, "if", self.id, windur)
        return self


class Chorddetect(Ugen):

    def __init__(self, reply_addr):
        super().__init__("Chorddetect", 0, NO_RATE, "s", None, True,
                         'reply_addr', reply_addr)

    def start(self, reply_addr):
        self.engine.send_cmd("/arco/chorddetect/start", 0, "is", self.id,
                        reply_addr)
        return self

    def set(self, input_name, value):
        self.inputs['input'] = value  # the only thing you can set is 'input'
        log.debug("Chorddetect set %d %d", self.id, value.id)
        self.engine.send_cmd("/arco/chorddetect/repl_input", 0, "ii", self.id,
                        value.id)
        return self


class SpectralCentroid(Ugen):

    def __init__(self, reply_addr):
        super().__init__("SpectralCentroid", 0, NO_RATE, "s", None, True,
                         'reply_addr', reply_addr)

    def start(self, reply_addr):
        self.engine.send_cmd("/arco/spectralcentroid/start", 0, "is", self.id,
                        reply_addr)
        return self

    def set(self, input_name, value):
        self.inputs['input'] = value  # the only thing you can set is 'input'
        log.debug("SpectralCentroid set %d %d", self.id, value.id)
        self.engine.send_cmd("/arco/spectralcentroid/repl_input", 0, "ii", self.id,
                        value.id)
        return self


class SpectralRolloff(Ugen):

    def __init__(self, reply_addr, threshold):
        super().__init__("SpectralRolloff", 0, NO_RATE, "sf", None, True,
                         'reply_addr', reply_addr, 'threshold', threshold)

    def start(self, reply_addr):
        self.engine.send_cmd("/arco/spectralrolloff/start", 0, "is", self.id,
                        reply_addr)
        return self

    def set(self, input_name, value):
        self.inputs['input'] = value  # the only thing you can set is 'input'
        log.debug("SpectralRolloff set %d %d", self.id, value.id)
        self.engine.send_cmd("/arco/spectralrolloff/repl_input", 0, "ii", self.id,
                        value.id)
        return self


class Stdistr(Ugen):

    def __init__(self, n, width):
        super().__init__("Stdistr", 2, A_RATE, "if", None, True, 'n', n,
                         'width', width)
        self.members = {}  # index -> Ugen: mirror server graph client-side

    def set_gain(self, gain):
        self.engine.send_cmd("/arco/stdistr/gain", 0, "if", self.id, gain)
        return self

    def set_width(self, width):
        self.engine.send_cmd("/arco/stdistr/width", 0, "if", self.id, width)
        return self

    def ins(self, index, ugen):
        self.members[index] = ugen
        self.engine.send_cmd("/arco/stdistr/ins", 0, "iii", self.id, index,
                             ugen.id)
        return self

    def rem(self, index):
        self.members.pop(index, None)
        self.engine.send_cmd("/arco/stdistr/rem", 0, "ii", self.id, index)
        return self


class Flsyn(Ugen):

    def __init__(self, path):
        super().__init__("Flsyn", None, A_RATE, "s", None, True, 'path',
                         path)  # already hard-coded as 2 channels in flsyn.h

    def alloff(self, chan):
        self.engine.send_cmd("/arco/flsyn/off", 0, "ii", self.id, chan)
        return self

    def control_change(self, chan, num, val):
        self.engine.send_cmd("/arco/flsyn/cc", 0, "iiii", self.id, chan, num, val)
        return self

    def channel_pressure(self, chan, val):
        self.engine.send_cmd("/arco/flsyn/cp", 0, "iii", self.id, chan, val)
        return self

    def key_pressure(self, chan, key, val):
        self.engine.send_cmd("/arco/flsyn/kp", 0, "iiii", self.id, chan, key, val)
        return self

    def noteoff(self, chan, key):
        self.engine.send_cmd("/arco/flsyn/noteoff", 0, "iii", self.id, chan, key)
        return self

    def noteon(self, chan, key, vel):
        self.engine.send_cmd("/arco/flsyn/noteon", 0, "iiii", self.id, chan, key,
                        vel)
        return self

    def pitch_bend(self, chan, bend):
        self.engine.send_cmd("/arco/flsyn/pbend", 0, "iif", self.id, chan, bend)
        return self

    def pitch_sens(self, chan, val):
        self.engine.send_cmd("/arco/flsyn/psens", 0, "iii", self.id, chan, val)
        return self

    def program_change(self, chan, program):
        self.engine.send_cmd("/arco/flsyn/prog", 0, "iii", self.id, chan, program)
        return self


class Wavetables(Ugen):

    def __init__(self, freq, amp, phase, chans, classname, rate):
        if chans is None:
            chans = max_chans(max_chans(1, freq), amp)
        super().__init__(classname, chans, rate, "UUf", None, None, 'freq',
                         freq, 'amp', amp, 'phase', phase)
        self.address_prefix = f"/arco/{self.classname.lower()}/"  # e.g. "/arco/tableosc/"

    def create_table(self, index, tlen, data, method_name):
        params = [self.id, index]
        type_str = "ii"

        if tlen is not None:  # omit length for time-domain data
            params.append(tlen)
            type_str += "i"

        # Add the float vector data
        params.extend(data)
        type_str += "f" * len(data)

        self.engine.send_cmd(f"{self.address_prefix}{method_name}", 0, type_str,
                        *params)
        return self

    def create_tas(self, index, tlen, ampspec):
        # Create table from amplitude spectrum
        return self.create_table(index, tlen, ampspec, "createtas")

    def create_tcs(self, index, tlen, spec):
        # Create table from complex spectrum (amplitude and phase pairs)
        return self.create_table(index, tlen, spec, "createtcs")

    def create_ttd(self, index, samps):
        # Create table from time-domain data (table length is len(samps))
        return self.create_table(index, None, samps, "createttd")

    def borrow(self, lender):
        self._lender = lender  # pin: server borrows lender's tables; keep it alive
        self.engine.send_cmd(f"{self.address_prefix}borrow", 0, "ii", self.id,
                        lender.id)
        return self

    def select(self, index):
        self.engine.send_cmd(f"{self.address_prefix}sel", 0, "ii", self.id, index)
        return self


class Tableosc(Wavetables):

    def __init__(self, freq, amp, phase=0, chans=None):
        super().__init__(freq, amp, phase, chans, "Tableosc", A_RATE)


class Tableoscb(Wavetables):

    def __init__(self, freq, amp, phase=0, chans=None):
        # Check rate requirements for b-rate oscillators
        if not isinstance(freq, (int, float)) and freq.rate != 'b':
            print("ERROR: 'freq' input to Ugen 'tableoscb' must be block rate")
            return None
        if not isinstance(amp, (int, float)) and amp.rate != 'b':
            print("ERROR: 'amp' input to Ugen 'tableoscb' must be block rate")
            return None
        super().__init__(freq, amp, phase, chans, "Tableoscb", B_RATE)


class Yin(Ugen):

    def __init__(self, input, minstep, maxstep, hopsize, address, chans=1):
        super().__init__("Yin", chans, A_RATE, "Uiiis", None, None, 'input',
                         input, 'minstep', minstep, 'maxstep', maxstep,
                         'hopsize', hopsize, 'address', address)


class Mix(Ugen):

    def __init__(self, chans=1, wrap=True):
        super().__init__("Mix", chans, A_RATE, "i", None, None, 'wrap',
                         1 if wrap else 0)

    def ins(self, name, ugen, gain, dur=0, mode=FADE_SMOOTH):
        if isinstance(gain, (int, float, list)):
            gain = Const(gain, None)
        elif gain.rate == A_RATE:
            print("WARNING: In Mix.ins, audio-rate mix gain is not allowed.")
            return self
        self.inputs[name] = [name, ugen, gain]
        self.engine.send_cmd("/arco/mix/ins", 0, "isiifi", self.id, str(name),
                        ugen.id, gain.id, dur, mode)
        return self

    def rem(self, name, dur=0, mode=FADE_SMOOTH):
        if name in self.inputs:
            self.engine.send_cmd("/arco/mix/rem", 0, "isfi", self.id, str(name),
                            dur, mode)
            del self.inputs[name]
        return self

    def find_name_of(self, ugen):
        for key, val in self.inputs.items():
            if isinstance(val, list) and val[1] == ugen:
                return key
        return None

    def set_gain(self, name, gain, chan=0):
        if name not in self.inputs:
            print("ERROR: Mix.set_gain() cannot find input", name)
            return self
        entry = self.inputs[name]
        gain_ugen = entry[2]
        if isinstance(gain_ugen, Ugen) and gain_ugen.rate == C_RATE:
            if chan >= gain_ugen.chans:
                print("WARNING: In Mix.set_gain(), gain for", name, "is a",
                      gain_ugen.chans, "channel Const, but set_gain requests"
                      " channel", chan)
            self.engine.send_cmd("/arco/mix/set_gain", 0, "isif", self.id,
                            str(name), chan, gain)
        else:
            if isinstance(gain, (int, float, list)):
                gain = Const(gain, None)
            elif gain.rate == A_RATE:
                print("WARNING: In Mix.set_gain(), audio-rate mix gain"
                      " will be downsampled and then interpolated")
            entry[2] = gain
            self.engine.send_cmd("/arco/mix/repl_gain", 0, "isi", self.id,
                            str(name), gain.id)
        return self


class Add(Ugen):

    def __init__(self, chans=1, wrap=True):
        super().__init__("Add", chans, A_RATE, "i", None, None, 'wrap',
                         1 if wrap else 0)
        self.members = {}  # id -> Ugen: mirror server graph client-side

    def ins(self, *ugens):
        for ugen in ugens:
            self.members[ugen.id] = ugen
            self.engine.send_cmd("/arco/add/ins", 0, "ii", self.id, ugen.id)
        return self

    def rem(self, ugen):
        self.members.pop(ugen.id, None)
        self.engine.send_cmd("/arco/add/rem", 0, "ii", self.id, ugen.id)
        return self

    def swap(self, ugen, replacement):
        self.members.pop(ugen.id, None)
        self.members[replacement.id] = replacement
        self.engine.send_cmd("/arco/add/swap", 0, "iii", self.id, ugen.id,
                             replacement.id)
        return self


class Addb(Ugen):

    def __init__(self, chans=1, wrap=True):
        super().__init__("Addb", chans, B_RATE, "i", None, None, 'wrap',
                         1 if wrap else 0)
        self.members = {}  # id -> Ugen: mirror server graph client-side

    def ins(self, ugen):
        self.members[ugen.id] = ugen
        self.engine.send_cmd("/arco/addb/ins", 0, "ii", self.id, ugen.id)
        return self

    def rem(self, ugen):
        self.members.pop(ugen.id, None)
        self.engine.send_cmd("/arco/addb/rem", 0, "ii", self.id, ugen.id)
        return self

    def swap(self, ugen, replacement):
        self.members.pop(ugen.id, None)
        self.members[replacement.id] = replacement
        self.engine.send_cmd("/arco/addb/swap", 0, "iii", self.id, ugen.id,
                             replacement.id)
        return self


class Onset(Ugen):

    def __init__(self, input, reply_addr):
        super().__init__("Onset", 0, A_RATE, "Us", None, True, 'input', input,
                         'reply_addr', reply_addr)


class Zero(Ugen):

    def __init__(self, id_num=None):
        super().__init__("Zero", 1, A_RATE, "", omit_chans=True,
                         id_num=id_num)


class Zerob(Ugen):

    def __init__(self, id_num=None):
        super().__init__("Zerob", 1, B_RATE, "", omit_chans=True,
                         id_num=id_num)


# Import auto-generated ugen wrappers (overrides hand-written versions if present)
try:
    from arco_generated import *
except ImportError:
    pass  # generated wrappers not yet built
