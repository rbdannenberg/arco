# pyarco — Python control bindings for Arco

`pyarco` is a pure-Python control-side client for [Arco](https://github.com/rbdannenberg/arco),
Roger Dannenberg's real-time audio synthesis and analysis engine. It is a
parallel implementation of the Serpent bindings in `serpent/srp/`: you build and
manipulate a graph of unit generators (ugens) from Python, and Arco does the DSP.

Communication is entirely over **O2** using
[`o2litepy`](https://github.com/rbdannenberg/o2) — a pure-Python O2lite
implementation. There is no C++ compilation and no shared library to load;
pyarco connects to a running Arco process as an O2 host. Ugens are addressed by
plain integer IDs chosen by the client, so no pointers cross the interface.

```
   your Python app  ──O2 messages──▶  Arco server  ──▶  audio out
   (pyarco: ArcoEngine, Ugen graph)   (C++ DSP engine)
```

---

## Requirements

- Python 3.8+
- `o2litepy` on the import path. In this repository it is vendored at
  `apps/test/python/o2litepy`, which `arco_engine.py` adds to `sys.path`
  automatically. (The import is deferred until `connect()`, so the modules and
  the offline test suite import fine even without it.)
- A running Arco server for live audio (e.g. `apps/basic` or the `apps/test`
  server). Not needed for the test suite.
- `pytest` for the test suite: `pip install -r requirements-dev.txt`.

---

## Quick start

```python
from arco import ArcoEngine, Sine

with ArcoEngine() as engine:          # connect(), create system ugens, set active
    tone = Sine(440, 0.2)             # freq, amp — numbers auto-wrap in Const ugens
    tone.play()                       # insert into the output Sum
    ...                               # keep a reference while it should sound
    tone.mute()                       # remove from output
# leaving the context calls engine.close(): frees every ugen, resets the pool
```

`ArcoEngine()` defaults to `input_chans=2, output_chans=2, ensemble="arco"`. Use
it as a context manager (above) or call `engine.connect()` / `engine.close()`
explicitly. `get_engine()` returns the active engine for code that doesn't hold a
direct reference.

A fuller, interactive tour lives in the NiceGUI demo at
`apps/test/python/init.py` (Connect, then play/mute/fade cards for oscillators,
filters, effects, envelopes, mixing, and math).

---

## Module layout

| File | Responsibility |
|------|----------------|
| `arco_engine.py` | `ArcoEngine` (O2 connection, weak-ref ugen registry, `UgenID` pool, action system), rate/op constants, and conversion/panning utilities |
| `arco_ugens.py` | `Ugen` base class and ~55 concrete wrapper classes |
| `arco_instr.py` | Instrument framework: `param()`/`Instrument`/`Synth`, `Note`/`Score`, `Reverb`, `Supersaw` |
| `arco.py` | Thin re-export shim so `from arco import Sine` works |
| `tests/` | Offline pytest suite (`FakeO2Lite` transport; no server needed) |
| `requirements-dev.txt` | Test dependency pin (`pytest`) |

---

## Ugen lifecycle model

This is the part most worth understanding, and where pyarco is opinionated.

**The registry is weak.** `ArcoEngine._ugens` is a
`weakref.WeakValueDictionary`. A ugen lives exactly as long as your Python code
(or the client-side graph) references it. When the last reference goes away,
`Ugen.__del__` runs, sends `/arco/free`, and returns the ID to the pool. There is
no session-long ID leak and no manual free step for the common case — dropping
the Python object is the free.

**The client mirrors the server graph** so nothing is collected while it is still
wired up and audible:

- Each ugen holds its inputs in `self.inputs` (a `Sine`'s `freq`/`amp` Consts,
  a filter's source, etc.).
- Container ugens track what has been inserted into them: `Sum`, `Sumb`, `Add`,
  `Addb`, `Route`, and `Stdistr` keep a `members` dict; `Mix` uses `self.inputs`.
- `play()` pins a ugen in `engine.output.members` until `mute()` — **audible
  implies alive**.
- `Thru.set_alternate`, `Recplay.borrow`, and `Wavetables.borrow` pin their
  argument (`_alternate` / `_lender`): the server-side reference must never
  outlive the client-side one.

**Ownership (`owns_id`).** Pool-allocated IDs are *owned* — registered, and freed
by `__del__`. IDs supplied explicitly (`id_num=`) are *borrowed*: the system
ugens at IDs 0–3, and `Instrument` wrappers that adopt their output ugen's ID.
Borrowed IDs are never registered and never freed by the wrapper, so wiring an
`Instrument` into the graph is identical to wiring its output ugen, with no
double-free.

**Fades.** `fade()` / `fade_in()` swap output membership and release the
terminated `Fader` shortly after the fade completes (`_server_freed` then
suppresses a redundant `/arco/free`). A second `fade()` on the same ugen adjusts
the in-flight fader instead of re-swapping; `mute()` during a fade targets the
live fader.

**Actions.** `atend()` / `register_action()` hold their target via `weakref` —
registering an action never keeps a ugen alive. Dead targets are pruned on
delivery, and one raising callback can't block delivery to the others.

Practical consequence: **keep a Python reference to anything you want to keep
hearing.** `engine.output` holds played ugens; container objects hold their
members; but a bare `Sine(440, 0.2)` with no `play()` and no saved reference will
be collected and freed.

---

## Constructing ugens

Concrete wrappers call the `Ugen` base constructor with a `types_` string, one
character per input:

- `"U"` — a ugen input. Numbers and lists are auto-wrapped in a `Const`.
- `"i"`/`"f"`/`"s"`/`"d"`/`"B"` — a literal int / float / string / double /
  boolean, sent as-is.

Inputs follow as alternating `(name, value)` pairs. Channel count defaults to the
max across inputs (`max_chans`), and single-channel inputs broadcast. Block-rate
variants (`Sineb`, `Mathb`, …) validate that their signal inputs are block- or
constant-rate at construction.

```python
class Sine(Ugen):
    def __init__(self, freq, amp, chans=None):
        if chans is None:
            chans = max_chans(max_chans(1, freq), amp)
        super().__init__("Sine", chans, A_RATE, "UU", None, None,
                         "freq", freq, "amp", amp)
```

O2 addresses follow `/arco/<class_lowercase>/<method>`: `/arco/sine/new`,
`/arco/<class>/set_<input>`, `/arco/<class>/repl_<input>`, `/arco/free`,
`/arco/sum/ins`, etc.

---

## Ugen catalog (~55 classes)

| Category | Classes |
|----------|---------|
| Base / abstract | `Ugen`, `Const_like`, `Envelope`, `Wavetables` |
| Constants / control | `Const`, `Smoothb` |
| Oscillators | `Sine`, `Sineb`, `Tableosc`, `Tableoscb` |
| Envelopes | `Pwl`, `Pwlb`, `Pwe`, `Pweb` |
| Fading | `Fader` |
| Filters / delays | `Delay`, `Allpass`, `Lowpass`, `Reson`, `Resonb`, `Feedback` |
| Blending | `Blend`, `Blendb` |
| Math | `Math`, `Mathb` (21 ops each), `Multx`, `Unary`, `Unaryb` (13 ops each) |
| Routing / mixing | `Sum`, `Sumb`, `Route`, `Mix`, `Add`, `Addb`, `Stdistr` |
| File / record | `Fileplay`, `Filerec`, `Recplay` |
| Granular / spectral | `Granstream`, `Pv`, `Ola_pitch_shift` |
| Analysis | `Vu`, `Trig`, `Probe`, `Yin`, `Onset`, `Chorddetect`, `SpectralCentroid`, `SpectralRolloff` |
| Downsampling | `Dnsampleb`, `Dualslewb` |
| MIDI / soundfont | `Flsyn` |
| System | `Zero`, `Zerob`, `Thru`, `Fanout` |

`Math` and `Unary` expose their operations as static constructors, e.g.
`Math.mult(a, b)`, `Math.add(a, b)`, `Unary.db_to_linear(x)`.

---

## Instrument framework (`arco_instr.py`)

Higher-level abstractions built on the ugen layer:

- **Parameters** — `param()`, `param_map()`, `param_method()` declare named,
  settable parameters backed by `Const`/`Smoothb` ugens or method calls, collected
  on a per-thread construction stack between `instr_begin()` and
  `Instrument.__init__()`.
- **`Instrument`** — wraps a ugen graph and adopts its output ugen's ID, so it
  wires into the graph like any ugen. Holds `parameter_bindings`.
- **`Synth`** — polyphonic note manager (`notes`, `free_notes`,
  `finishing_notes`). `noteon()` creates or recycles an instrument and mixes it
  in; `noteoff()` releases it; subclasses implement `instr_create()`.
- **`Note` / `Score`** — simple event model with `append`, `merge`, `stretch`,
  and blocking `play()`.
- **`Reverb` / `Multi_reverb`** — comb + allpass reverb ported from Nyquist,
  with configurable RT60, wet/dry, and lowpass cutoff.
- **`Supersaw_instr` / `Supersaw_synth`** — multi-oscillator supersaw (detune,
  animate, rolloff, anti-aliased wavetables, LFO vibrato, lowpass, envelope) —
  the reference example of the full Instrument/Synth pattern.

Conversion and panning helpers live in `arco_engine.py` and are re-exported:
`hz_to_step` / `step_to_hz` / `step_to_ratio` / `ratio_to_step` /
`steps_to_hzdiff`, `db_to_linear` / `linear_to_db`, `vel_to_linear` /
`linear_to_vel` / `vel_to_db` / `db_to_vel`, and `pan_linear` / `pan_eqlpow` /
`pan_45`.

---

## Signal rates

| Rate | Symbol | Meaning |
|------|--------|---------|
| Audio | `A_RATE = 'a'` | one sample per audio frame (block of 32 @ 44.1 kHz) |
| Block | `B_RATE = 'b'` | one sample per block (~1378 Hz) |
| Constant | `C_RATE = 'c'` | a float updated by control messages, stored in a `Const` |

Many ugens ship in audio/block pairs (`Sine`/`Sineb`, `Math`/`Mathb`). A
block-rate input accepts numbers, `Const` (c-rate) ugens, and b-rate ugens —
it rejects only a-rate signals.

---

## Testing

The suite is **offline**: a `FakeO2Lite` transport records the O2 messages each
call would send, so tests assert on message contents, ID-pool accounting, and
garbage-collection behavior without a running Arco server (and without
`o2litepy`).

```bash
pip install -r requirements-dev.txt
python -m pytest pyarco/tests -v      # 64 passed
```

Coverage spans the message protocol (`test_smoke`, `test_ugens`), the constants
(`test_constants`), the lifecycle machinery — weak registry, ownership, fades,
borrow pinning (`test_lifecycle`) — the action system (`test_actions`), the
instrument framework and per-thread construction (`test_instr`), the block-rate
guards (`test_rate_guards`), and a full `Supersaw_synth` noteon/noteoff cycle
(`test_supersaw`).

---

## Known follow-ups

- `/actl/act` is not yet registered as an o2lite method handler, and nothing
  calls `atend()` yet, so server-initiated actions (`Instrument.finish` /
  `Synth.is_finished` note recycling) do not fire against a live server. The
  handler and `Synth.is_finished` themselves are implemented and tested.
- `term(dur)` outside the fade helpers still desyncs the client ID pool from the
  server: the server frees the ID at termination, but the client slot stays
  allocated until the Python object is dropped. A full fix needs client-side
  `ACTION_FREE` handling.

---

## Relationship to the Serpent bindings

pyarco mirrors the `serpent/srp/` API — same ugen names, same O2 message
protocol — for teams that prefer Python (e.g. NiceGUI front-ends, existing Python
tooling) over Serpent. The `.ugen` source files remain the authoritative
interface definition; a future `.ugen → .py` generator (see the standalone
pyarco repo) would emit these wrappers the way `preproc/u2f.py` emits the Serpent
ones. Until then the wrappers here are hand-written and validated against the
Serpent behavior.
