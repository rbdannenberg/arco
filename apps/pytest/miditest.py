"""miditest.py - a minimal example now that PyArco implementation has settled down.

Run with "PYTHON_PATH=../.. python3 miditest.py" in this directory.
You need pyarco and o2lite to be subdirectories of the PYTHON_PATH.
You also need to run server built using CMakeLists.txt in this directory.

This program depends on soundfont.py to define a path to a sound font,
e.g., I use FluidR3_GM.sf2. You need to get the file and define the
path, e.g., my directory has soundfont.py with:

SOUNDFONT = "/Users/<myid>/fluidsynth/FluidR3_GM/FluidR3_GM.sf2"

but your path will obviously be different. The path is sent to the Arco
audio process, so if you use a relative path, it should be based on
where you run the audio process, not on where you run this Python script.
"""

from pyarco.arco import *  # you might want to be more selective
from pyarco import sched
from pyarco.steps import step_to_hz
from soundfont import SOUNDFONT
from random import random
from allugens import *  # this gets definitions for all ugens in dspmanifest.txt
# but when dspmanifest.txt is updated, you need to make the project again to
# update allgens and also make sure the server has the DSP code compiled in.
# we import allugens rather than "from pyarco.ugens import *" because that
# might import classes that have no corresponding DSP implementation.


class MidiSender:
    """Creates MIDI messages and forwards them to a receiver. The receiver
    in this implementation is a FluidSynth unit generator in Arco. Not
    implemented, but a nice extension would be to enable sending to an
    O2 service that forwards to a "real" MIDI device, OR sending to another
    object that would interpret MIDI in order to control some other Arco
    Instrument.

    This class also accepts "raw" MIDI messages.
    """
    def __init__(self, flsyn):
        """Pass in an Flsyn object (Flsyn is a subclass of Ugen that
        controls an Arco Fluidsynth unit generator object.
        """
        self.target = flsyn

    def noteon(self, chan, key, vel):
        self.target.noteon(chan, key , vel)

    def noteoff(self, chan, key, vel):
        # Arco/FluidSynth ignore note-off velocity
        self.target.noteoff(chan, key)

    def control_change(self, chan, num, val):
        self.target.control_change(chan, num, val)

    def channel_pressure(self, chan, val):
        self.target.channel_pressure(chan, val)

    def key_pressure(self, chan, key, val):
        self.target.key_pressure(chan, key, val)

    def pitch_bend(self, chan, bend):
        """bend is -1 to +1"""
        self.target.pitch_bend(chan, bend)

    def pitch_sens(self, chan, val):
        self.target.pitch_sens(chan, val)

    def program_change(self, chan, program):
        self.target.program_change(chan, program)

    def alloff(self, chan):
        self.target.alloff(chan)

    def midi_osc_fmt(self, msg):
        """decode and act on a MIDI message encoded in OSC or O2 format.
        msg is stored as 3 bytes: status, data1, data2, from
        high-order to low-order. Note that this is different
        from PortMIDI, which reverses the order.
        """
        status = (msg >> 24) & 0xFF
        cmd = status & 0xF0
        chan = status & 0xF
        data1 = (msg >> 16) & 0xFF
        data2 = (msg >> 8) & 0xFF
        if cmd == 0x90:
            if data2 == 0:
                target.noteoff(chan, data1, data2)
            else:
                target.noteon(chan, data1, data2)
        elif cmd == 0x80:
            target.noteoff(chan, data1, data2)
        elif cmd == 0xB0:
            target.control_change(chan, data1, data2)
        elif cmd == 0xD0:
            target.channel_pressure(chan, data1)
        elif cmd == 0xA0:
            target.key_pressure(chan, data1, data2)
        elif cmd == 0xE0:
            bend = ((data2 << 7 + data1) / 0x2000) + 1.0
            self.pitch_bend(chan, bend)
        elif cmd == 0xC0:
            target.program_change(chan, data1)
        # other MIDI messages are ignored

    def pitch_bend_range(self, chan, val):
        """Set the range of pitch bend to -val to +val,
        indicated by -1 to +1 values of bend in pitch_bend()
        """
        target.pitch_sens(chan, val)


MAJOR_SCALE = [0, 2, 4, 5, 7, 9, 11]
seq_playing = False
seq_id = 0
seq_key = 0
seq_prev_pitch = 60
seq_chan = 0

def seq_play_note(my_id):
    """It's time to play a note (unless seq_id has changed)"""
    global seq_id, seq_key, seq_prev_pitch, midi_sender
    if my_id != seq_id:
        return  # end of this sequence
    # pick an interval. power gives small-interval bias:
    interval = 2 + int(11 * (random() ** 4))  # +2 avoids unisons
    if seq_prev_pitch > 60 and random() > 0.3:  # bias toward middle-C
        interval = -interval
    pitch = seq_prev_pitch + interval
    # quantize pitch to major scale in key. Quantization is easy because if
    # you are not in the scale, then both pitch+1 and pitch-1 are in the scale
    if (pitch + 12 - seq_key) % 12 not in MAJOR_SCALE:
        pitch += 1
    midi_sender.noteoff(seq_chan, seq_prev_pitch, 0)
    midi_sender.noteon(seq_chan, pitch, 100)
    seq_prev_pitch = pitch
    # bias toward short durations and inter-onset intervals:
    sched.cause(random() ** 2 + 0.1, None, seq_play_note, my_id)


def play_sequence(param):
    """Play random notes from a major scale chosen by key.
    Any call with param in 0:11 will start in key given by param.
    Stop with param == 12.
    Change programs with param > 12.
    """
    global seq_id, seq_key, midi_sender
    if param < 0:
        return  # ignore invalid values
    elif param < 12:
        seq_key = param
        print("Transpose by", param)
        if not seq_playing:  # start playing
            seq_id += 1
            seq_play_note(seq_id)
    elif param == 12:  # end playing
        seq_id += 1
        print("Stopping sequence")
    else:
        # mapping param to programs assumes we'll only provide a small
        # number of values above 12 and I want to map to a fairly wide
        # range of program values since general midi has a lot of similar
        # keyboard sounds in the low program numbers
        program = ((param - 13) * 5) % 128  # pick a program
        midi_sender.program_change(seq_chan, program)
        print("Program change to", program)
    

def play_now():
    """called when Arco is initialized and ready for commands"""
    # create the Fluidsynth unit generator
    global flsyn_ugen, midi_sender
    flsyn_ugen = Flsyn(SOUNDFONT)
    flsyn_ugen.play()
    midi_sender = MidiSender(flsyn_ugen)
    play_sequence(0)  # start playing immediately


def terminal_input_handler(ch):
    """called when user types a character"""
    global seq_prev_pitch, seq_chan
    print(ch)
    chi = ord(ch)
    if chi >= ord('a') and chi <= ord('z'):
        play_sequence(chi - ord('a'))
    elif ch == '?':
        print(f"There are {arco_ids.free_count()} arco IDs in the free list.")
    elif ch == 'Q':
        play_sequence(12)  # stop the sequencer from creating new notes
        flsyn_ugen.noteoff(seq_chan, seq_prev_pitch)  # current note off
        sched.cause(0.1, None, arco.finish)  # allow time for note off
        # in 0.1 sec, arco.finish() is called, causing sched.run() to return
        # to main(), which will exit


def main():
    global io
    async_terminal_input(terminal_input_handler)
    flags = "" if len(sys.argv) < 2 else sys.argv[1]
    arco.initialize(o2_debug_flags=flags, when_ready=play_now)
    print("\nStarting miditest.py: a-l transpose, m stop, >m: program change")
    print("    ? prints how many IDs are free.")
    print("    Q to exit.")
    if flags:
        print("miditest initialized o2lite with o2_debug_flags", flags)
    else:
        print("You can pass o2_debug_flags, e.g. rs, on the command line.\n")
    sched.run(poll_period_ms=2)


main()
