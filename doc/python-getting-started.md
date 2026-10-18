# Getting Started with Arco from Python

Arco can be controlled from Python. Instead of linking Arco into
wxSerpent, you run Arco as a stand-alone server in one terminal and
run Python programs in another. The Python programs find the server
automatically on your computer and send it commands to create and
control sound. You can leave the server running and start, stop, and
edit Python programs as often as you like.

```
Terminal 1                         Terminal 2
-----------------------            ----------------------------
pytestserver                <--    python3 mintest.py
(audio engine, prints log)  O2lite (makes and controls sound)
```

The Python library is `pyarco` (in `arco/pyarco/`). It talks to the
server using `o2litepy` (in `arco/o2litepy/`), a pure-Python
implementation of O2lite, which discovers the server on the local
machine using Zeroconf.

These instructions were written on macOS. The server build depends on
the libraries built by the main installation, so complete
[the MacOS installation](installation.md#installation-on-macos) first.
In particular, `arco/apps/common/libraries.txt` must exist. It is
created by `arco/build_everything_cmd.sh`.

The examples below assume your build folder is `~/build` (with
`~/build/arco`, `~/build/o2`, `~/build/fluidsynth`, ...). Use your own
folder name if it is different.

## 1. Build the server

`apps/pytest` builds a server named `pytestserver` together with the
Python example programs:

    cd ~/build/arco/apps/pytest
    cmake . -DCMAKE_BUILD_TYPE=Debug -DUSE_O2=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    make

Check that both of these were created in `apps/pytest`:

    ls -lh pytestserver allugens.py

- `pytestserver` is the server program.
- `allugens.py` is generated from this folder's `dspmanifest.txt`. It
  defines Python classes for exactly the unit generators compiled into
  this server, which is why the examples say `from allugens import *`.
  If you add a unit generator to `dspmanifest.txt`, re-run `cmake` and
  `make` so that the server and `allugens.py` stay in sync.

The many `was built for newer 'macOS' version` linker warnings are
harmless.

## 2. Set up Python (one time)

`o2litepy` needs two packages, `zeroconf` and `netifaces`. Install them
in a virtual environment so they don't affect the rest of your system:

    cd ~/build/arco
    python3 -m venv .venv
    source .venv/bin/activate
    pip install zeroconf netifaces

Your prompt now starts with `(.venv)`. In any `new` terminal, run
`source ~/build/arco/.venv/bin/activate` again before running examples.

## 3. Terminal 1: start the server

    cd ~/build/arco/apps/pytest
    ./pytestserver

The server prints a log. It reads its saved settings
(`arco_server_prefs.json`) and waits for a client. When a Python program
connects, the program resets Arco and asks it to start audio, and you
will see something like:

    /host/clear received. Closing reset in progress.
    /host/run received. Starting audio devices.
    ...
        Arco selects input device: MacBook Pro Microphone
            WARNING: only 1 input channels available.
        Arco selects output device: MacBook Pro Speakers
        Audio latency = 10 ms
    ...
    Audio open completed successfully.

The warning just means the built-in microphone is mono while Arco asked
for stereo input. It is harmless.

Leave this terminal running. The directory you start the server in
matters: any relative file path a Python program sends (sound files,
soundfonts) is opened by the server, relative to this directory.

## 4. Terminal 2: run your first program

Open a second terminal:

    cd ~/build/arco
    source .venv/bin/activate
    cd apps/pytest
    PYTHONPATH=../.. python3 mintest.py

`PYTHONPATH=../..` points Python at the `arco` folder, which contains
both `pyarco` and `o2litepy`. You should see:

    Connected to ensemble arco at O2time 54.895.
    Arco reset completed.

    Starting mintest.py: a-z play notes, ? prints how many IDs are free.
        Q to exit.

and hear a one-second FM tone. Press letters `a`-`z` to play notes
at different pitches, `?` to print how many unit generator IDs are
free, and `Q` (capital) to quit. The server keeps running, so you can
start another program right away.

## 5. How the program works

The heart of `mintest.py` is a function that builds a small patch of
unit generators for each note:

```python
def play_tone(pitch, amp, dur):
    freq = step_to_hz(pitch)            # MIDI pitch to Hz
    im = 30                             # index of modulation
    modulator = sine(freq, im * freq)
    env = pwlb(0.01, 0.2, dur).term()   # envelope that ends after dur
    fm = mult(sine(add(mult(modulator, env), freq), 1.0), env)
    fm.term()                           # end when an input (env) ends
    fm.play()                           # connect to the audio output
```

- Each call like `sine(...)` or `mult(...)` creates a unit generator on
  the server and returns a Python object that refers to it.
- `pwlb(...)` is a piecewise-linear envelope: rise to 0.2 in 0.01 s,
  then fall to 0 over `dur` seconds.
- `.term()` means "terminate when an input terminates". When the
  envelope ends, the whole patch ends and the server frees it, so notes
  clean up after themselves.
- `.play()` connects the result to the audio output.

The program starts with:

```python
arco.initialize(o2_debug_flags=flags, when_ready=play_now)
sched.run(poll_period_ms=2)
```

`arco.initialize` finds the server and calls `play_now` once Arco is
ready. `sched.run` runs the scheduler and polls for messages until the
program calls `arco.finish()`.

## 6. The other examples

Run each one the same way from Terminal 2, with the server still
running in Terminal 1.

### pytest.py: a collection of tests

    PYTHONPATH=../.. python3 pytest.py

Press any unlisted key (for example `q`) to print the menu:

| Key | Action |
|---|---|
| a / b | play a simple tone (two variants) |
| c / d | start / stop an additive tone (440 Hz, 6 harmonics) |
| e | toggle a repeating sequence of additive tones |
| f | start a test of a note with a 1 second decay |
| g | reset Arco |
| 0-9 | play pitched test notes |
| Q | quit |

### miditest.py: a General MIDI synthesizer (FluidSynth)

This example needs a file `soundfont.py` in `apps/pytest` that gives the
full path to a soundfont. The installation downloads one to
`fluidsynth/FluidR3_GM/` in your build folder:

    echo 'SOUNDFONT = "'$HOME'/build/fluidsynth/FluidR3_GM/FluidR3_GM_GS.sf2"' > soundfont.py
    cat soundfont.py
    PYTHONPATH=../.. python3 miditest.py

It plays a repeating note sequence. Press `a`-`l` to transpose,
`m` to stop, `>` to change the instrument (program change), `?` to
print free IDs, and `Q` to quit. Loading the 144 MB soundfont can take
a second or two.

### soundfiletest.py: playing sound files

This example plays two `mono` sound files, `sndtest0.wav` and
`sndtest1.wav`, from the server's directory. On macOS you can make test
files with the `say` command:

    say -o sndtest0.wav --data-format=LEI16@44100 "This is sound file zero"
    say -o sndtest1.wav --data-format=LEI16@44100 "And this is sound file one"
    PYTHONPATH=../.. python3 soundfiletest.py

File 0 plays when the program starts. Press `a` to play file 0,
`b` to play file 1, and `Q` to quit. After each file ends, the
program prints a `playback_end_callback` line. That is the server
telling Python the file finished, which the program uses to get the
player ready again.

`soundfont.py` and the `.wav` files are your own local files; don't
commit them.

## 7. Troubleshooting

| What you see | What it means | What to do |
|---|---|---|
| Python hangs and never prints `Connected to ensemble arco` | It can't find the server. | Make sure `pytestserver` is running in Terminal 1. Check that macOS allows Terminal to use the local network (System Settings > Privacy & Security > Local Network). |
| `ModuleNotFoundError: No module named 'pyarco'` or `'o2litepy'` | `PYTHONPATH` doesn't point at the `arco` folder. | Run from `apps/pytest` with `PYTHONPATH=../..` (note: `PYTHONPATH`, one word). |
| `ModuleNotFoundError: No module named 'zeroconf'` | The virtual environment isn't active. | `source ~/build/arco/.venv/bin/activate` |
| `ModuleNotFoundError: No module named 'soundfont'` | `miditest.py` needs `soundfont.py`. | Create it as shown above. |
| Connected, but no sound | Audio isn't running, or the wrong device is selected. | Look at Terminal 1 for `Audio open completed successfully` and the selected output device. |
| Many lines like `__del__ on <sine Ugen, id 293 ...> from old epoch 2 while current epoch is 3.` | Expected, not an error. Each Arco reset starts a new "epoch". These are Python objects made before a reset being cleaned up after it; pyarco skips freeing them on the server because the reset already did. | Nothing. |
| `include could not find requested file: .../apps/common/libraries.txt` when running `cmake` | The main installation hasn't generated `libraries.txt` (or it was deleted, e.g. by `git clean`). | Run `./build_everything_cmd.sh` in `arco/`. |
| `"_iconv"` undefined when linking `pytestserver` | `libraries.txt` points iconv at Homebrew's libiconv. | Set `ICONV_OPT_LIB` and `ICONV_DBG_LIB` to `$(xcrun --show-sdk-path)/usr/lib/libiconv.tbd`. |
