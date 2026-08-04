"""soundfiletest.py - a minimal example now that PyArco implementation has settled down.

Run with "PYTHON_PATH=../.. python3 soundfiletest.py" in this directory.
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
from allugens import *  # this gets definitions for all ugens in dspmanifest.txt
# but when dspmanifest.txt is updated, you need to make the project again to
# update allgens and also make sure the server has the DSP code compiled in.
# we import allugens rather than "from pyarco.ugens import *" because that
# might import classes that have no corresponding DSP implementation.

# These sound files are NOT included in the project. Use any MONO file you
# like, and use absolute paths if possible. Relative paths are relative to
# the current working directory of the server, not necessarily this Python
# script.
FILE0 = "./sndtest0.wav"
FILE1 = "./sndtest1.wav"

# These are the soundfile players. fileplay0 is refreshed and always on
# standby to play a soundfile. fileplay1 is created on demand and persists
# until the file finished, at which time the unit generator is freed and
# the variable is set back to None:
fileplay0 = None
fileplay1 = None


def playback_end_callback(status, uid, extra_data):
    """Handler for when file playback reaches the end.
    In this case, we simply replace free Fileplay object and make a 
    new one ready to play the file again.

    Normally, you would arco.register_action() passing an object and
    method to call back, but since this is a function, object is None.
    But since object is None, we need information on which fileplay
    the callback is for. We pass 0 or 1 as extra_data as the last
    parameter to arco.register_action().
    """
    global fileplay0, fileplay1
    print("playback_end_callback: status", status, "extra_data", extra_data)
    if status & ACTION_END == 0:
        return  # not an end action (ignore ACTION_FREE and others)
    if extra_data == 0:
        fileplay0.mute()  # disconnect from output so it can be freed
        fileplay0 = fileplay(FILE0, 1)  # get file ready to play again
        arco.register_action(fileplay0, ACTION_END, None, 
                             playback_end_callback, 0)
        fileplay0.play()  # connect new fileplay0 to output
        print("readying fileplay0:", fileplay0)
    elif fileplay1 and extra_data == 1:
        fileplay1.mute()  # disconnect from output so it can be freed
        fileplay1 = None  # indicate that the fileplay is no longer in use
        print("stopping fileplay1")
    else:
        print("unexpected unit generator id", uid, "expected", fileplay0.id_num)


def play_file(filenum):
    """Play sound file. For lowest latency, we keep a Fileplay object
    for FILE0 ready to play. For FILE1, we create a Fileplay object on
    demand, showing a simpler but potentially higher-latency method. Even
    the on-demand approach is very fast, especially with low-latency
    solid-state storage.
    """
    global fileplay0, fileplay1
    if filenum == 0:  # fileplay is ready to roll
        fileplay0.start(True)
    elif filenum == 1:  # fileplay is created on demand
        if fileplay1 is not None:
            # if we replace fileplay1, the ACTION_END handler will not have
            # a reference to this current fileplay1, so it will stop playing
            # but will never be freed. So we (crudely) disconnect it from
            # audio output and let garbage collection remove it:
            fileplay1.mute()  # disconnect from output so it can be freed
            # it would be nicer to smoothly ramp this to zero and then free it:
            # see fade methods.
        fileplay1 = fileplay(FILE1, 1)  # one channel
        fileplay1.play()
        arco.register_action(fileplay1, ACTION_END, None,
                             playback_end_callback, 1)
        fileplay1.start()


def play_now():
    """called when Arco is initialized and ready for commands"""
    # create the Fluidsynth unit generator
    global fileplay0, fileplay1
    fileplay0 = fileplay(FILE0, 1)
    arco.register_action(fileplay0, ACTION_END, None, playback_end_callback, 0)
    fileplay0.play()
    # give a little time for file to preload
    sched.cause(0.1, None, play_file, 0)


def terminal_input_handler(ch):
    """called when user types a character"""
    global seq_prev_pitch, seq_chan
    print(ch)
    chi = ord(ch)
    if chi == ord('a'):
        play_file(0)
    elif chi == ord('b'):
        play_file(1)
    elif ch == '?':
        print(f"There are {arco_ids.free_count()} arco IDs in the free list.")
    elif ch == 'Q':
        # to shut down cleanly, delete references to fileplay0
        sched.cause(0.1, None, arco.finish)  # allow time for note off
        # in 0.1 sec, arco.finish() is called, causing sched.run() to return
        # to main(), which will exit
    else:
        print("a to play file 0, b to play file 1, Q to quit.")


def main():
    global io
    async_terminal_input(terminal_input_handler)
    flags = "" if len(sys.argv) < 2 else sys.argv[1]
    arco.initialize(o2_debug_flags=flags, when_ready=play_now)
    print("\nStarting soundfiletest.py: a or b to play file.")
    print("    ? prints how many IDs are free.")
    print("    Q to exit.")
    if flags:
        print("soundfiletest initialized o2lite with o2_debug_flags", flags)
    else:
        print("You can pass o2_debug_flags, e.g. rs, on the command line.\n")
    sched.run(poll_period_ms=2)


main()
