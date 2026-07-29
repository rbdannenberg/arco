import sys
import time
import math
import re
import weakref
import platform
from .terminput import TermInput
from .ugens.zero import Zero, Zerob
from .ugens.thru import Thru
from .ugens.sum import Sum
from .arco_ugens import *
from o2litepy import o2lite
from . import sched

# --- Action registration system ---

class Ugen_action:
    def __init__(self, target, method, parameters):
        self.target = weakref.ref(target)
        assert isinstance(method, str), (
               f"Ugen_action method {method} must be a string.")
        self.method = method
        self.parameters = parameters
        # print("Created", self)


    def __repr__(self):
        refcnt = 0
        if self.target():
            refcnt = sys.getrefcount(self.target())
        return (f"<Ugen_action {self.target()} ({refcnt}) {self.method!r}>")


class Action_list:
    def __init__(self, action_mask, ugen_actions):
        self.action_mask = action_mask
        self.ugen_actions = ugen_actions

    def __str__(self):
        return (f"<Action_list: mask {repr(self.action_mask)}"
                f" acts {repr(self.ugen_actions)}>")


# --------------- ArcoEngine ---------------

arco_allowed_state_transitions = {  # maps current state to allowed new states
    'init': ['discovery', 'quitting'],
    'discovery': ['initializing', 'quitting'],
    'initializing': ['devinf', 'quitting'],
    'initializing2': ['devinf2', 'quitting'],
    'devinf': ['opening', 'stopped', 'quitting'],
    'devinf2': ['stopped', 'quitting'],
    'stopped': ['stopped', 'opening', 'devinf2', 'initializing2', 'quitting'],
    'opening': ['stopped', 'running', 'quitting'],
    'running': ['stopped', 'quitting']}


def o2lite_time_get():
    return o2lite.time_get()

def o2lite_poll():
    o2lite.poll()


class ArcoEngine:

    def __init__(self):
        self.input_chans = None   # must set with initialize
        self.output_chans = None  # must set with initialize
        self.action_dict = {}
        self.next_action_id = 1
        self.zero = None
        self.zerob = None
        self.input = None
        self.output = None
        self.state = 'init'
        self.arco_run_set = []  # UGens that run but not used for audio output
        self.call_when_ready = None
        return None


    def initialize(self, ensemble="arco", input_chans=2, output_chans=2,
                   o2_debug_flags="", network=True, timeout=30, when_ready=None):
        global o2lite
        print("o2lite", o2lite)
        if o2lite and o2lite.time_get() > 0:
            return None  # already started

        # complete circular reference: arco_ugens needs a reference to arco,
        # but it cannot simply import arco_engine at the top-level because
        # arco_engine.py (this file) imports arco_ugens. Here, we simply
        # pass our arco to arco_ugens:
        arco_ugens_initialize(self)

        self.input_chans = input_chans
        self.output_chans = output_chans
        o2lite.initialize(ensemble, debug_flags=o2_debug_flags)
        o2lite.set_services("actl")  # arco control messages come here
        o2lite.method_new("/actl/act", "iii", True, actl_act_handler, None)
        o2lite.method_new("/actl/nougen", "s", True, actl_nougen_handler, None)
        o2lite.method_new("/actl/reset", "", True, actl_reset_handler, None)
        o2lite.method_new("/actl/started", "", True, actl_started_handler, None)
        deadline = time.time() + timeout
        while o2lite.time_get() < 0:
            if time.time() > deadline:
                raise TimeoutError("Could not connect to Arco server within "
                    f"{timeout} seconds. Is the Arco server running?")
            o2lite.poll()
            time.sleep(0.01)
        print("Connected to ensemble", ensemble, "O2time", o2lite.time_get())
        sched.poll_function_add(o2lite_poll)

        self.reset(when_ready)
        #  callback to /actl/reset will signal that arco has been reset
        self.zero = None  # use zero as signal that reset is completed
        deadline = time.time() + 5
        while self.zero is None:
            if time.time() > deadline:
                raise TimeoutError("Could not reset Arco server within 5 "
                                   "seconds. Is the arco server running?")
            o2lite.poll()
            time.sleep(0.01)
        # when we return, reset has completed and initial ugens for
        # zero and input/output are constructed and installed


    def reset_completed_callback(self, status):
        if status != 0:
            print("!!!! ERROR: arco.reset_completed_handler status",
                  status, "!!!!")
            print("!!!! Untested code path !!!!")
        # System ugen shadows - pass in fixed id numbers for ugens
        arco_ids.new_epoch()  # releases old python Ugen objects if any
        self.action_dict.clear()
        self.zero = Zero(ZERO_ID)
        self.zerob = Zerob(ZEROB_ID)
        self.input = Thru(self.input_chans, self.zero, INPUT_ID)
        self.output = Sum(self.output_chans, True, OUTPUT_ID)
        self.start_audio(True)


    def started_callback(self, status):
        if (status != 0):
            print("!!!! ERROR: Error opening audio.",
                  "Reconfigure audio and restart.")
            exit(1)
        # now initialize scheduler - sched.poll functions remain intact
        #   but schedulers are initialized and time base is reset here:
        sched.time_get = o2lite_time_get
        sched.init()
        # normally, schedulers both start at t=0. Here, we bump rtsched
        # to start at the current O2 time.  vtsched is initialized so that
        # virttime=0 maps to realtime=0, but we need to change that so
        # virttime=0 maps to realtime="current O2 time":
        sched.rtsched.time = o2lite_time_get()  # sync rtsched to O2
        sched.rtsched.time_offset = 0.0
        sched.vtsched.rt_base = sched.rtsched.time  # fix vtime mapping
        if self.call_when_ready is not None:
            self.call_when_ready()


    def reset(self, readyfn=None):
        o2lite.send_cmd("/host/clear", 0, "")
        self.call_when_ready = readyfn


    def run(self):
        while True:
            o2lite.poll()
            time.sleep(0.002)


    def register_action(self, ugen, action_mask, target, method, *parameters):
        """Register a callback action for ugen events.

        On the audio thread, Ugen objects have an optional action_id
        and action_mask. When "actions" occur, such as termination or
        input is ready, send_action_id() is called, which compares the
        action to the action_mask. If not masked, a message is sent to
        /actl/act with the action_id, status, and Ugen id.

        On this client side, we request these messages with /arco/act,
        providing the Ugen id, the action_id, and the desired mask.

        See arco/doc/pyarco.md for more discussion.
        """
        action = Ugen_action(target, method, parameters)
        aid = ugen.action_id
        if aid is not None:
            action_list = self.action_dict.get(aid)
            if action_list is None:
                print("ERROR: register_action - action_id not in action_dict",
                      aid)
                return
            if action_mask != (action_mask & action_list.action_mask):
                action_list.action_mask = action_list.action_mask | action_mask
                o2lite.send_cmd("/arco/act", 0, "iii", ugen.arco_ref(), aid,
                              action_list.action_mask)
            action_list.ugen_actions.append(action)
        else:
            aid = self.next_action_id
            al = Action_list(action_mask, [action])
            self.action_dict[aid] = al
            ugen.action_id = aid
            o2lite.send_cmd("/arco/act", 0, "iii", ugen.arco_ref(),
                          self.next_action_id, action_mask)
            self.next_action_id += 1
        # print(f"registered aid {aid} list {self.action_dict[aid]}")
        

    def set_arco_state(self, newstate: str):
        """This function checks if a transition to newstate is legal. If legal,
        arco_state is changed and return value is true. Otherwise, stay in the
        current state, print a warning and return false.
        """
        allowed = arco_allowed_state_transitions[self.arco_state]
        if newstate not in allowed:
            print("*****************************************************")
            print("* WARNING: Unexpected state transition from",
                  self.arco_state, "to", newstate,
                  "-- keeping", self.arco_state)
            return False
        print("arco_state change to", newstate)
        self.arco_state = newstate
        return True


    def start_audio(self, run_stop: bool):
        """Turn audio processing on (True) or off (False)"""
        o2lite.send_cmd("/host/run", 0, "i", 1 if run_stop else 0)



def actl_act_handler(address, types, info):
    """Handler for /actl/act messages from Arco server. An act(ion) message
    consists of:
        action_id - when a Ugen (on this side) wants to receive notices from
            Arco service, it sends a unique action_id and a mask to indicate
            what notices are requested. The action_id is stored on the Arco
            Ugen and returned in this /actl/act message.
        status - an int with bits set according to state of the Ugen, e.g.,
            ACTION_TERM means the Ugen has terminated; ACTION_FREE means the
            Ugen reference count went to zero and it was freed.
        uid - an optional Ugen ID, e.g., when an input to Sum terminates,
            Sum sends ACTION_REM with the input ID. -1 means "no ID provided"
    """
    key = o2lite.get_int32()
    status = o2lite.get_int32()
    uid = o2lite.get_int32()
    al = arco.action_dict.get(key)
    # print("actl_act_handler", address, al, key, status, uid)
    if al is None:
        return
    i = 0
    while i < len(al.ugen_actions):
        action = al.ugen_actions[i]
        target = action.target()  # deref weak reference
        if target:
            method = action.method
            getattr(target, method)(status, uid, action.parameters)
            i = i + 1
        else:
            al.ugen_actions.pop(i)

    if (status & ACTION_FREE) > 0:
        arco.action_dict.pop(key, None)
        return


def actl_reset_handler(address, types, info):
    """Handler for /actl/reset messages from Arco server"""
    status = o2lite.get_int32()
    arco.reset_completed_callback(status)


def actl_started_handler(address, types, info):
    """Handler for /actl/started messages from Arco server"""
    status = o2lite.get_int32()
    arco.started_callback(status)


def actl_nougen_handler(address, types, info):
    """handles nougen message from arco"""
    msg = o2lite.get_string()
    print("********************************************")
    print("* ERROR:", msg)
    match = re.search(r'/arco/(.*?)/new', msg)
    if match:
        print("*     It looks like the Arco server was not linked with the",
              match.group(1), "unit generator")
        print("*     Check the Arco server's dspmanifest.txt file and rebuild.")
    print("********************************************")


# Inverse transitions: for each next state, allowed current state
#    'discovery': [nil],
#    'initializing': 'discovery',
#    'devinf': 'initializing',
#    'opening': ['devinf', 'stopped'],
#    'running': 'opening',
#    'stopped': ['opening', 'running', 'devinf', 'devinf2'],
#    'devinf2': ['initializing2', 'stopped'],
#    'initializing2': 'stopped'}


# atend types for Mix.ins():
SIGNAL = 'signal'
GAIN = 'gain'
BOTH = 'both'
# atend types for ugen.atend(). Various Ugens can send notification of
# end events: Pwlb, Fileplay, ...
MUTE = 'mute'      # standard thing to do at end: call mute() to delete
                   #     a sound instance
FINISH = 'finish'  # custom behavior - the target should implement finish()
                   #     to handle the event

# manage action callbacks
next_action_id = 1
action_dict = {}

max_ugen_id_used = 0
ugens_created = 0


dbg_exit_function = 'arco_quit'  # shut down arco if debugger exits program


arco_open_msg = ""
if platform.system() == "Linux":
   arco_open_msg = (" If Arco crashes (on Linux) after opening audio, "
                    "try increasing latency to 100 msec or opening other "
                    "device(s).")

# ------------- Async Terminal Input ----------------
#
# Since the client might want to use TermInput, but also might call
# exit(), we provide a "safe" implementation to make it easy:
# call async_terminal_input(callbackfn) to start, and
# async_terminal_input(None) to stop. Cleanup is automatic through
# atexit (although not bulletproof).

_terminput_callback = None  # TermInput is stopped if None
_io = None  # the TermInput object
_terminput_polling = False  # polling set up yet?


def _terminput_poll():
    global _io, _terminput_callback
    if _io:
        input = _io.getch()
        if input and _terminput_callback is not None:
            _terminput_callback(input)
        

def async_terminal_input(callbackfn):
    global _terminput_callback, _io, _terminput_polling
    
    if not _io and callbackfn is not None:
        _io = TermInput()
        # atexit.register(_terminput_shutdown)
        if not _terminput_polling:
            sched.poll_function_add(_terminput_poll)
    
    if _io and _terminput_callback is not None and callbackfn is None:
        _io.stop()
    elif _io and _terminput_callback is None and callbackfn is not None:
        _io.start()
    _terminput_callback = callbackfn


arco = ArcoEngine()
