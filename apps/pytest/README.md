# apps/pytest: Python examples for the Arco server

This folder builds `pytestserver`, a stand-alone Arco server that runs in
a terminal, and contains Python programs that control it:

- `mintest.py` - the smallest example: plays FM notes from the keyboard
- `pytest.py` - a collection of tests (tones, sequences, reset)
- `miditest.py` - a General MIDI synthesizer using FluidSynth
  (needs a local `soundfont.py`)
- `soundfiletest.py` - plays sound files (needs `sndtest0.wav` and
  `sndtest1.wav`)

Quick start, after the main installation is complete:

    # build (once)
    cmake . -DCMAKE_BUILD_TYPE=Debug -DUSE_O2=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    make

    # Terminal 1
    ./pytestserver

    # Terminal 2 (with a virtual environment that has zeroconf and netifaces)
    PYTHONPATH=../.. python3 mintest.py

See [doc/python-getting-started.md](../../doc/python-getting-started.md)
for the full walkthrough, the keys each example uses, and
troubleshooting.
