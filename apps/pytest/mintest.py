"""mintest.py - a minimal example now that PyArco implementation has settled down.

Run with "PYTHON_PATH=../.. python3 mintest.py" in this directory. You need pyarco
and o2lite to be subdirectories of the PYTHON_PATH.

"""

from pyarco.arco import *  # you might want to be more selective
from pyarco.steps import step_to_hz
from allugens import *  # this gets definitions for all ugens in dspmanifest.txt
# but when dspmanifest.txt is updated, you need to make the project again to
# update allgens and also make sure the server has the DSP code compiled in.


def play_tone(pitch, amp, dur):
    """Play a tone. This is a simple approach that does not reuse
    unit generators and allocates new ones for every sound. For
    elaborate sounds with many Ugens, this can result in audible
    delays when multiple notes are played at once.

    pitch - MIDI pitch number
    amp - amplitude (linear)
    dur - in seconds
    """

    # a 1-channel mixer that terminates when there are no more inputs
    # the_mix = mix(1).term()  
    
    freq = step_to_hz(pitch)
    im = 30  # index of modulation
    modulator = sine(freq, im * freq)
    env = pwlb(0.01, 0.2, dur).term()
    fm = mult(sine(add(mult(modulator, env), freq), 1.0), env)
    fm.term()  # will terminate when an input terminates, one input is
                    # env, who's term() causes termination when the end of
                    # the envelope is reached, so everything is cleaned up
                    # after dur
    fm.play()


def play_now():
    """called when Arco is initialized and ready for commands"""
    play_tone(60, 0.2, 1.0)


def terminal_input_handler(ch):
    """called when user types a character"""
    print(ch)
    chi = ord(ch)
    if chi >= ord('a') and chi <= ord('z'):
        play_tone(36 + chi - ord('a'), 0.2, 1.0)
    elif ch == '?':
        print(f"There are {arco_ids.free_count()} arco IDs in the free list.")


def main():
    global io
    async_terminal_input(terminal_input_handler)
    flags = "" if len(sys.argv) < 2 else sys.argv[1]
    arco.initialize(o2_debug_flags=flags, when_ready=play_now)
    if flags:
        print("mintest initialized o2lite with o2_debug_flags", flags)
    else:
        print("You can pass o2_debug_flags, e.g. rs, on the command line.")
    print("mintest - a-z play notes, ? prints how many IDs are free.")
    sched.run(poll_period_ms=2)


main()
