# pytest.py -- simple test program for pyarco
#
# Roger B. Dannenberg
# Jan 2022

import random
import re
from arco_engine import arco
from pan import stereoize
from steps import *
from allugens import *
from arco_instr import *
import sched
from sched import rtsched, vtsched, absolute, real_delay
from terminput import TermInput

# call arco_init(ensemble) to start
# call arco_run() after setting up UI and or scheduling things
# arco_ready is set to true when arco is ready for action

SIMPLE = False

class Atone (Instrument):

    def __init__(self, freq, dur, nharm):
        print("Atone: freq", freq, "dur", dur, "nharm", nharm)
        instr_begin()
        self.renv = None
        self.env = pwlb(dur * 0.2, 0.01, dur * 0.8)
        self.env.term()  # allow envelope to terminate; termination
        # will propagate all the way to the output, which will
        # remove and free the instrument (assuming there are no
        # other references to it, which we will avoid).
        the_mix = mix(1).term()
        for i in range(nharm):
            # we need a symbol for each mixer input but since the
            # number of inputs is controlled by nharm, we have to
            # manufacture symbols:
            name = mix_name(i)
            the_mix.ins(name, sine(freq * i, 0.8 ** i), self.env)
        if not SIMPLE and random.choice([True, False]):
            # reson filter center frequency envelope:
            self.renv = pwlb(random.random(), random.uniform(4, 10) * freq,
                        5.0, 200.0, max(5.0, dur * 2))
            q = random.uniform(0.5, 2.0)
            the_mix = reson(the_mix, self.renv, q)
        super().__init__("Atone", the_mix)


    def start(self):
        self.env.start()
        if self.renv:  # start the reson filter envelope too if it exists
            self.renv.start()
        return self

atone_running = False

def atone_sequence():
    global atone_running
    print("atone_sequence called")
    step = round(random.uniform(40, 80))
    freq = step_to_hz(step)
    assert isinstance(freq, float), f"type of freq {type(freq)}"
    dur = random.uniform(0.5, 10)
    nharm = 1 if SIMPLE else round(random.uniform(2, 17))
    # print("atone_sequence: freq", freq, "dur", dur, "nharm", nharm,
    #       "sched.current", sched.current, "vtsched", vtsched, 
    #       "sched.vtime", sched.vtime)
    # careful here: start() is a method of Atone, but we must "play"
    # the Ugen returned by stereoize. (We could have put stereoize
    # inside Atone, but we didn't.)  There's no race condition where
    # envelopes could start before we're connected to output because
    # until we are connected to some sink (like audio output), there
    # will be no calls to compute samples, so envelopes will sit and
    # wait even if we get connected to output long after .start().
    stereoize(Atone(freq, dur, nharm).start()).play()
    ioi = random.uniform(0.25, 2.0) ** 2
    if atone_running:
        print("scheduling atone_sequence after", ioi)
        print("    sched module time", sched.time_get())
        print("    o2lite time", o2lite.time_get())
        print("    sched.rtime", sched.rtime)
        print("    sched.vtime", sched.vtime)
        sched.cause(ioi, None, atone_sequence)

atone_running = False

def atone_sequence_start(keychar):
    global atone_running
    if not atone_running:
        atone_running = True
        print("Starting atone_sequence")
        atone_sequence()
    else:
        print("Stopping atone_sequence")
        atone_running = False


ui_initialized = False
ready_count = 0

def arco_ready():
# called when arco is intialized and ready to make sound or when
# arco audio stream has been reopened
    global ready_count
    print("**** arco audio is running! ****  ready_count", ready_count)
    ready_count = ready_count + 1
    if ready_count > 1:
        print("arco_ready called again: ready_count", ready_count)
    print("    current time: ", sched.time_get())
    # term_test()
    # play_simple_test(True)
    # o2lite.sleep(1)
    # play_simple_test(False)



######### Synthesis Tests ##############

simple_test_on = False

def play_pitched(keychar):
    freq = 440.0 * 2 ** (int(keychar) / 12)
    play_simple_test(True, freq)
    o2lite.sleep(0.5)
    play_simple_test(False)


atone = None

def atone_start(keychar):
    global atone
    atone = Atone(440.0, 4.0, 6).start().play()

def atone_stop(keychar):
    global atone
    atone.mute()
    atone = None


# handler for stcheckbox: play/stop some sine tones
def play_simple_test(x, freq=440.0):
    global sin1, sin2, sin3, sin4, simple_test_on
    print("play_simple_test: x", x)
    if x and not simple_test_on:
        sin1 = sine(freq, 0.01).play()
        sin2 = sine(freq * 2, 0.01).play()
        sin3 = sine(freq * 3, 0.007).play()
        sin4 = sine(freq * 4, 0.005).play()
        simple_test_on = True
    elif simple_test_on:
        sin1.mute()
        sin1 = None  # delete reference for garbage collection
        sin2.mute()
        sin2 = None
        sin3.mute()
        sin3 = None
        sin4.mute()
        sin4 = None
        simple_test_on = False


def make_simple_tone(dur = 3):
    env = pweb(0.5, 1, dur, 1, dur + 1, start = True, lin = True)
    env.term()
    sin1 = sine(440.0, 0.01)
    sin2 = sine(880.0, 0.01)
    sin3 = sine(1320.0, 0.007)
    sin4 = sine(1760.0, 0.005)
    stone = sum().ins(sin1, sin2, sin3, sin4)
    enved = mult(stone, env)
    print("make_simple_tone: enved", enved, end="")
    print(" id_num", hex(enved.id_num))
    return enved


simple_tone_on = False

# handler for simtoncheckbox: play/stop some sine tones
def play_simple_tone(x=True, keychar=None):
# this is a cleanup test: in this version, env termination propagates
# through mult to the output sum, which releases the calculation tree
# after unchecking the box releases simton_out.
    global simpton_out, simple_tone_on
    print("play_simple_tone: x", x)
    if x and not simple_tone_on:
        simton_out = make_simple_tone().play()
        simple_tone_on = True
    elif simple_tone_on:
        simton_out = None
        simple_tone_on = False


def stop_simple_tone(keychar=None):
    play_simple_tone(False)


class Finishtest:
    """This is not a good example of how to do anything "right", but it 
    exercises some code for atend, FINISH, and cleanup. When created,
    global finish_test gets the object holding `out`, so the calculation
    tree has a reference and is retained, but when box is unchecked,
    finish_test is cleared and envelope ramps down. When it ends, 
    finish should mute the output and everything is garbage collected.
    """

    def __init__(self):
        # envelope (need reference for noteoff):
        self.env = pweb(0.5, 1, start = True, lin = True)
        # self.env.atend(FINISH, self)
        sin1 = sine(440.0, 0.01)
        sin2 = sine(880.0, 0.01)
        sin3 = sine(1320.0, 0.007)
        sin4 = sine(1760.0, 0.005)
        sum_ugen = sum().ins(sin1, sin2, sin3, sin4)
        self.out = mult(sum_ugen, self.env).term().play()  # output ugen

    def __del__(self):
        print("Finishtest.__del__")

    def noteoff(self):
        self.env.decay(1).term()
        print("started 1 sec decay")

    # not used:
    def finish(self, status, finisher, parameters):
        print("Finishtest.finish: status", status, "status & ACTION_END", 
              status & ACTION_END, "finisher", finisher,
              "parameters", parameters)
        if (status & ACTION_END) == 0:
            print("**** WARNING: finish called but not on ACTION_END")
        self.out.mute()


finish_test_running = False

def play_finish_test(keychar=None):
# this is a test for atend(FINISH, self): when box is unchecked, noteoff
# sends ending to envelope, which sends finish() to instrument, which 
    global finish_test, finish_test_running
    if finish_test_running:
        finish_test.noteoff()
        finish_test = None
    else:
        finish_test = Finishtest()  # make finish_test
    finish_test_running = not finish_test_running


# here's what happens at end (depends on GC timing though):
# - when finish_test is created, there are two action_id's registered:
#   - finish_test is target when env finishes
#   - sum is target when mult finishes
# - after starting the test, user unchecks the box
# - finish_test is deleted and is GC'd before env terminates
# - env starts decay and .term() method sets its CAN_TERMINATE flag
# - env reaches end; it is input to a mult
# - "finish" is sent to finish_test, but ignored because it was GC'd
# - mult gets termination from env
# - sum removes mult and sends an ACTION_REMOVE
# - sum has no reference, so it sends an ACTION_FREE, but it was GC'd
# - env is freed and sends an ACTION_FREE, to finish_test, but env was GC'd

def term_test(keychar=None):
    sin1 = sine(440.0, 0.01)
    env = pweb(0.5, 1, 2, 0, start = True, lin = True).term()
    mult1 = mult(sin1, env).term().play()
    print(f"term_test completed: sin1 {sin1.arco_ref()}"
          f" env {env.arco_ref()} mult1 {mult1.arco_ref()}")


help_strings = []
commands = {}

def add_command(help_string, func):
    help_strings.append(help_string)
    commands[help_string[0]] = func

def print_help():
    for help in help_strings:
        print(help)

def terminal_input_poll():
    input = io.getch()
    if input:
        print("terminal input got: ", repr(input))
        key = "0" if input in "0123456789" else input
        func = commands.get(input, None)
        if func is None:
            print_help()
        else:
            func(keychar=input)


def reset_arco(keychar):
    arco.reset()

def quit_pytest(keychar):
    global io
    sched.stop()
    io.stop()
    arco.reset(exit)


def main():
    global io
    io = TermInput()
    io.start()
    arco.initialize(o2_debug_flags="")
    add_command("a - play_simple_tone(True)", play_simple_tone)
    add_command("b - play_simple_tone(False)", stop_simple_tone)
    add_command("c - play an Atone()", atone_start)
    add_command("d - stop the Atone()", atone_stop)
    add_command("e - toggle atone_sequence", atone_sequence_start)
    add_command("f - toggle play_finish_test", play_finish_test)
    add_command("g - reset arco", reset_arco)
    add_command("q - quit", quit_pytest)
    add_command("0-9 - play_simple_test pitched", play_pitched)
    # when arco is connected and running, call arco_ready():
    arco_ready()  # our application start-up code after initialization
    # sched.cause(2.0, None, play_finish_test, True)
    # sched.cause(4.0, None, play_finish_test, False)
    sched.poll_function_add(terminal_input_poll)
    sched.run(poll_period_ms=5)


main()
