import os
import sys
import math

sys.path.insert(0, os.path.abspath(
    os.path.join(os.path.dirname(__file__), "../../../pyarco")))

from nicegui import ui, app, run

import arco
from arco import (
    Sine,
    Sineb,
    Const,
    Smoothb,
    Fader,
    Delay,
    Feedback,
    Allpass,
    Reson,
    Lowpass,
    Blend,
    Sum,
    Mix,
    Route,
    Pwl,
    Pwlb,
    Pwe,
    Pweb,
    Pv,
    Granstream,
    Ola_pitch_shift,
    Math,
    Mathb,
    Unary,
    Unaryb,
    Multx,
    Tableosc,
    Tableoscb,
    Dnsampleb,
    Dualslewb,
    Flsyn,
    Stdistr,
    Vu,
    Trig,
    Yin,
    Chorddetect,
    SpectralCentroid,
    SpectralRolloff,
    Zero,
    Zerob,
    A_RATE,
    B_RATE,
    C_RATE,
    AR,
    FADE_LINEAR,
    FADE_EXPONENTIAL,
    FADE_LOWPASS,
    FADE_SMOOTH,
    BLEND_LINEAR,
    BLEND_POWER,
    BLEND_45,
    MATH_OP_MUL,
    MATH_OP_ADD,
    MATH_OP_SUB,
    MATH_OP_DIV,
    MATH_OP_MAX,
    MATH_OP_MIN,
    MATH_OP_CLP,
    MATH_OP_POW,
    DNSAMPLE_BASIC,
    DNSAMPLE_AVG,
    DNSAMPLE_PEAK,
    DNSAMPLE_RMS,
    ACTION_END,
    ACTION_TERM,
    ACTION_END_OR_TERM,
    MUTE,
    FINISH,
    create_fader,
    hz_to_step,
    step_to_hz,
    vel_to_linear,
    db_to_linear,
    pan_linear,
    pan_eqlpow,
    pan_45,
    max_chans,
    ArcoEngine,
    OUTPUT_ID,
)

# ═══════════════════════ State management ═══════════════════════


class DemoState:
    """Holds all active UGens and connection status."""

    def __init__(self):
        self.connected = False
        self.engine = None
        self.active_ugens = {}  # tag -> {'ugen': Ugen, 'playing': bool, ...}
        self._counter = 0

    def tag(self, prefix="ugen"):
        self._counter += 1
        return f"{prefix}_{self._counter}"

    def register(self, tag, ugen, playing=False, **extra):
        self.active_ugens[tag] = {'ugen': ugen, 'playing': playing, **extra}

    def get(self, tag):
        return self.active_ugens.get(tag)

    def remove(self, tag):
        entry = self.active_ugens.pop(tag, None)
        if entry and entry['playing']:
            try:
                entry['ugen'].mute()
            except Exception:
                pass
        return entry

    def connect(self):
        if not self.connected:
            try:
                self.engine = ArcoEngine()
                self.engine.connect()
                self.connected = True
            except Exception as e:
                self._connect_error = str(e)
        return self.connected


state = DemoState()

# ═══════════════════════ UI helpers ═══════════════════════


def status_chip(playing: bool):
    if playing:
        return '🔊 Playing'
    return '🔇 Stopped'


def ensure_connected():
    if not state.connected:
        ui.notify('Not connected to Arco server. Click Connect first.',
                  type='warning')
        return False
    return True


def make_card_header(title, description=''):
    with ui.row().classes('items-center gap-2 w-full'):
        ui.label(title).classes('text-lg font-bold')
        if description:
            ui.label(description).classes('text-xs text-gray-500')


# ═══════════════════════ Oscillator demos ═══════════════════════


def sine_demo():
    tag = state.tag('sine')
    ugen_ref = {'obj': None, 'playing': False}
    status_label = None

    with ui.card().classes('w-full'):
        make_card_header('Sine', 'Audio-rate sine oscillator')
        ui.separator()

        freq_slider = ui.slider(min=20, max=4000, value=440, step=1) \
            .props('label-always').classes('w-full')
        ui.label().bind_text_from(freq_slider,
                                  'value',
                                  backward=lambda v: f'Frequency: {v} Hz')

        amp_slider = ui.slider(min=0, max=1.0, value=0.3, step=0.01) \
            .props('label-always').classes('w-full')
        ui.label().bind_text_from(amp_slider,
                                  'value',
                                  backward=lambda v: f'Amplitude: {v:.2f}')

        with ui.row().classes('items-center gap-4'):
            status_label = ui.label(status_chip(False))

            def toggle_play():
                if not ensure_connected():
                    return
                if ugen_ref['playing']:
                    ugen_ref['obj'].mute()
                    ugen_ref['playing'] = False
                    state.active_ugens[tag]['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if ugen_ref['obj'] is None:
                        ugen_ref['obj'] = Sine(freq_slider.value,
                                               amp_slider.value)
                        state.register(tag, ugen_ref['obj'])
                    ugen_ref['obj'].play()
                    ugen_ref['playing'] = True
                    state.active_ugens[tag]['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle_play) \
                .props('color=primary')

            def cleanup():
                if ugen_ref['obj'] is not None:
                    state.remove(tag)
                    ugen_ref['obj'] = None
                    ugen_ref['playing'] = False
                    status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_freq_change(e):
            if ugen_ref['obj'] and ugen_ref['playing']:
                ugen_ref['obj'].set('freq', e.args)

        def on_amp_change(e):
            if ugen_ref['obj'] and ugen_ref['playing']:
                ugen_ref['obj'].set('amp', e.args)

        freq_slider.on('update:model-value', on_freq_change)
        amp_slider.on('update:model-value', on_amp_change)


def tableosc_demo():
    tag = state.tag('tableosc')
    ugen_ref = {'obj': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Tableosc', 'Wavetable oscillator')
        ui.separator()

        freq_slider = ui.slider(min=20, max=4000, value=440, step=1) \
            .classes('w-full')
        ui.label().bind_text_from(freq_slider,
                                  'value',
                                  backward=lambda v: f'Frequency: {v} Hz')

        amp_slider = ui.slider(min=0, max=1.0, value=0.3, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(amp_slider,
                                  'value',
                                  backward=lambda v: f'Amplitude: {v:.2f}')

        harmonics_slider = ui.slider(min=1, max=32, value=8, step=1) \
            .classes('w-full')
        ui.label().bind_text_from(harmonics_slider,
                                  'value',
                                  backward=lambda v: f'Harmonics: {v}')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle_play():
                if not ensure_connected():
                    return
                if ugen_ref['playing']:
                    ugen_ref['obj'].mute()
                    ugen_ref['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if ugen_ref['obj'] is None:
                        t = Tableosc(freq_slider.value, amp_slider.value)
                        n = int(harmonics_slider.value)
                        ampspec = [1.0 / (i + 1) for i in range(n)]
                        t.create_tas(0, max(512, 16 * n), ampspec)
                        t.select(0)
                        ugen_ref['obj'] = t
                        state.register(tag, t)
                    ugen_ref['obj'].play()
                    ugen_ref['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle_play) \
                .props('color=primary')

            def cleanup():
                if ugen_ref['obj'] is not None:
                    state.remove(tag)
                    ugen_ref['obj'] = None
                    ugen_ref['playing'] = False
                    status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_freq(e):
            if ugen_ref['obj'] and ugen_ref['playing']:
                ugen_ref['obj'].set('freq', e.args)

        def on_amp(e):
            if ugen_ref['obj'] and ugen_ref['playing']:
                ugen_ref['obj'].set('amp', e.args)

        freq_slider.on('update:model-value', on_freq)
        amp_slider.on('update:model-value', on_amp)


# ═══════════════════════ Filter demos ═══════════════════════


def lowpass_demo():
    tag = state.tag('lowpass')
    refs = {'source': None, 'filter': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Lowpass',
                         'First-order lowpass filter on a sine source')
        ui.separator()

        src_freq = ui.slider(min=20, max=4000, value=440,
                             step=1).classes('w-full')
        ui.label().bind_text_from(src_freq,
                                  'value',
                                  backward=lambda v: f'Source freq: {v} Hz')

        cutoff_slider = ui.slider(min=20, max=16000, value=2000, step=10) \
            .classes('w-full')
        ui.label().bind_text_from(cutoff_slider,
                                  'value',
                                  backward=lambda v: f'Cutoff: {v} Hz')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['filter'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(src_freq.value, 0.4)
                        refs['filter'] = Lowpass(refs['source'],
                                                 cutoff_slider.value)
                        state.register(tag, refs['filter'])
                    refs['filter'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, filter=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_cutoff(e):
            if refs['filter'] and refs['playing']:
                refs['filter'].set('cutoff', e.args)

        def on_src(e):
            if refs['source'] and refs['playing']:
                refs['source'].set('freq', e.args)

        cutoff_slider.on('update:model-value', on_cutoff)
        src_freq.on('update:model-value', on_src)


def reson_demo():
    tag = state.tag('reson')
    refs = {'source': None, 'filter': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Reson', 'Resonant bandpass filter')
        ui.separator()

        src_freq = ui.slider(min=20, max=4000, value=220,
                             step=1).classes('w-full')
        ui.label().bind_text_from(src_freq,
                                  'value',
                                  backward=lambda v: f'Source freq: {v} Hz')

        center_slider = ui.slider(min=20, max=8000, value=880, step=10) \
            .classes('w-full')
        ui.label().bind_text_from(center_slider,
                                  'value',
                                  backward=lambda v: f'Center: {v} Hz')

        q_slider = ui.slider(min=0.5, max=100, value=10, step=0.5) \
            .classes('w-full')
        ui.label().bind_text_from(q_slider,
                                  'value',
                                  backward=lambda v: f'Q: {v:.1f}')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['filter'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(src_freq.value, 0.3)
                        refs['filter'] = Reson(refs['source'],
                                               center_slider.value,
                                               q_slider.value)
                        state.register(tag, refs['filter'])
                    refs['filter'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, filter=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_center(e):
            if refs['filter'] and refs['playing']:
                refs['filter'].set('center', e.args)

        def on_q(e):
            if refs['filter'] and refs['playing']:
                refs['filter'].set('q', e.args)

        center_slider.on('update:model-value', on_center)
        q_slider.on('update:model-value', on_q)


def allpass_demo():
    tag = state.tag('allpass')
    refs = {'source': None, 'effect': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header(
            'Allpass',
            'Allpass delay -- adds phase shift without changing amplitude spectrum'
        )
        ui.separator()

        src_freq = ui.slider(min=100, max=2000, value=440, step=1) \
            .classes('w-full')
        ui.label().bind_text_from(src_freq,
                                  'value',
                                  backward=lambda v: f'Source freq: {v} Hz')

        dur_slider = ui.slider(min=0.001, max=0.1, value=0.01, step=0.001) \
            .classes('w-full')
        ui.label().bind_text_from(dur_slider,
                                  'value',
                                  backward=lambda v: f'Delay: {v:.3f} s')

        fb_slider = ui.slider(min=0, max=0.95, value=0.7, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(fb_slider,
                                  'value',
                                  backward=lambda v: f'Feedback: {v:.2f}')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['effect'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(src_freq.value, 0.3)
                        refs['effect'] = Allpass(refs['source'],
                                                 dur_slider.value,
                                                 fb_slider.value, 0.2)
                        state.register(tag, refs['effect'])
                    refs['effect'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, effect=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_dur(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set('dur', e.args)

        def on_fb(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set('fb', e.args)

        dur_slider.on('update:model-value', on_dur)
        fb_slider.on('update:model-value', on_fb)


# ═══════════════════════ Effects demos ═══════════════════════


def delay_demo():
    tag = state.tag('delay')
    refs = {'source': None, 'effect': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Delay', 'Delay line with feedback')
        ui.separator()

        src_freq = ui.slider(min=100, max=2000, value=440,
                             step=1).classes('w-full')
        ui.label().bind_text_from(src_freq,
                                  'value',
                                  backward=lambda v: f'Source freq: {v} Hz')

        dur_slider = ui.slider(min=0.01, max=1.0, value=0.25, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(dur_slider,
                                  'value',
                                  backward=lambda v: f'Delay: {v:.2f} s')

        fb_slider = ui.slider(min=0, max=0.95, value=0.5, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(fb_slider,
                                  'value',
                                  backward=lambda v: f'Feedback: {v:.2f}')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['effect'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(src_freq.value, 0.3)
                        refs['effect'] = Delay(refs['source'],
                                               dur_slider.value,
                                               fb_slider.value, 1.0)
                        state.register(tag, refs['effect'])
                    refs['effect'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, effect=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_dur(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set('dur', e.args)

        def on_fb(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set('fb', e.args)

        dur_slider.on('update:model-value', on_dur)
        fb_slider.on('update:model-value', on_fb)


def blend_demo():
    tag = state.tag('blend')
    refs = {'s1': None, 's2': None, 'blend': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Blend', 'Crossfade between two sine sources')
        ui.separator()

        freq1 = ui.slider(min=100, max=2000, value=440,
                          step=1).classes('w-full')
        ui.label().bind_text_from(freq1,
                                  'value',
                                  backward=lambda v: f'Source 1 freq: {v} Hz')

        freq2 = ui.slider(min=100, max=2000, value=660,
                          step=1).classes('w-full')
        ui.label().bind_text_from(freq2,
                                  'value',
                                  backward=lambda v: f'Source 2 freq: {v} Hz')

        b_slider = ui.slider(min=0, max=1.0, value=0.5, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(
            b_slider,
            'value',
            backward=lambda v: f'Blend: {v:.2f} (0=src1, 1=src2)')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['blend'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['s1'] is None:
                        refs['s1'] = Sine(freq1.value, 0.4)
                        refs['s2'] = Sine(freq2.value, 0.4)
                        refs['blend'] = Blend(refs['s1'], refs['s2'],
                                              b_slider.value, BLEND_POWER)
                        state.register(tag, refs['blend'])
                    refs['blend'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(s1=None, s2=None, blend=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_blend(e):
            if refs['blend'] and refs['playing']:
                refs['blend'].set('b', e.args)

        b_slider.on('update:model-value', on_blend)


def pitch_shift_demo():
    tag = state.tag('pitchshift')
    refs = {'source': None, 'effect': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('OLA Pitch Shift', 'Overlap-add pitch shifting')
        ui.separator()

        src_freq = ui.slider(min=100, max=2000, value=440,
                             step=1).classes('w-full')
        ui.label().bind_text_from(src_freq,
                                  'value',
                                  backward=lambda v: f'Source freq: {v} Hz')

        ratio_slider = ui.slider(min=0.5, max=2.0, value=1.0, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(ratio_slider,
                                  'value',
                                  backward=lambda v: f'Ratio: {v:.2f}x')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['effect'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(src_freq.value, 0.3)
                        refs['effect'] = Ola_pitch_shift(
                            refs['source'], ratio_slider.value, 0.02, 0.04)
                        state.register(tag, refs['effect'])
                    refs['effect'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, effect=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_ratio(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set_ratio(e.args)

        ratio_slider.on('update:model-value', on_ratio)


def granstream_demo():
    tag = state.tag('granstream')
    refs = {'source': None, 'effect': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Granstream', 'Granular synthesis on a live source')
        ui.separator()

        src_freq = ui.slider(min=100, max=2000, value=220,
                             step=1).classes('w-full')
        ui.label().bind_text_from(src_freq,
                                  'value',
                                  backward=lambda v: f'Source freq: {v} Hz')

        density_slider = ui.slider(min=1, max=50, value=10, step=1) \
            .classes('w-full')
        ui.label().bind_text_from(density_slider,
                                  'value',
                                  backward=lambda v: f'Density: {v}')

        dur_slider = ui.slider(min=0.01, max=0.5, value=0.1, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(dur_slider,
                                  'value',
                                  backward=lambda v: f'Grain dur: {v:.2f} s')

        ratio_lo = ui.slider(min=0.5, max=2.0, value=0.8, step=0.05) \
            .classes('w-full')
        ui.label().bind_text_from(ratio_lo,
                                  'value',
                                  backward=lambda v: f'Ratio low: {v:.2f}')

        ratio_hi = ui.slider(min=0.5, max=2.0, value=1.2, step=0.05) \
            .classes('w-full')
        ui.label().bind_text_from(ratio_hi,
                                  'value',
                                  backward=lambda v: f'Ratio high: {v:.2f}')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['effect'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(src_freq.value, 0.3)
                        refs['effect'] = Granstream(refs['source'],
                                                    int(density_slider.value),
                                                    dur_slider.value, True)
                        refs['effect'].set_ratio(ratio_lo.value,
                                                 ratio_hi.value)
                        state.register(tag, refs['effect'])
                    refs['effect'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, effect=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        def on_density(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set_density(e.args)

        def on_dur(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set_dur(e.args)

        def on_ratio(e):
            if refs['effect'] and refs['playing']:
                refs['effect'].set_ratio(ratio_lo.value, ratio_hi.value)

        density_slider.on('update:model-value', on_density)
        dur_slider.on('update:model-value', on_dur)
        ratio_lo.on('update:model-value', on_ratio)
        ratio_hi.on('update:model-value', on_ratio)


# ═══════════════════════ Envelope demos ═══════════════════════


def pwl_demo():
    tag = state.tag('pwl')
    refs = {'source': None, 'env': None, 'out': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header(
            'PWL / PWE',
            'Piecewise linear or exponential envelope applied to sine')
        ui.separator()

        freq_slider = ui.slider(min=100, max=2000, value=440,
                                step=1).classes('w-full')
        ui.label().bind_text_from(freq_slider,
                                  'value',
                                  backward=lambda v: f'Frequency: {v} Hz')

        attack_slider = ui.slider(min=0.001, max=1.0, value=0.05, step=0.001) \
            .classes('w-full')
        ui.label().bind_text_from(attack_slider,
                                  'value',
                                  backward=lambda v: f'Attack: {v:.3f} s')

        sustain_slider = ui.slider(min=0.01, max=3.0, value=0.5, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(sustain_slider,
                                  'value',
                                  backward=lambda v: f'Sustain: {v:.2f} s')

        release_slider = ui.slider(min=0.01, max=2.0, value=0.3, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(release_slider,
                                  'value',
                                  backward=lambda v: f'Release: {v:.2f} s')

        env_type = ui.toggle({
            0: 'PWL (linear)',
            1: 'PWE (exponential)'
        },
                             value=0).classes('w-full')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def trigger():
                if not ensure_connected():
                    return

                # Clean up previous envelope/output to avoid leaking UGens
                if refs['out'] is not None:
                    try:
                        refs['out'].mute()
                    except Exception:
                        pass
                    refs['out'] = None
                    refs['env'] = None

                atk = attack_slider.value
                sus = sustain_slider.value
                rel = release_slider.value
                points = [atk, 1.0, sus, 1.0, rel, 0.0]

                if refs['source'] is None:
                    refs['source'] = Sine(freq_slider.value, 1.0)

                if env_type.value == 0:
                    refs['env'] = Pwl(points, start=False)
                else:
                    refs['env'] = Pwe(points, initial_value=0.001, start=False)

                refs['out'] = Math.mult(refs['source'], refs['env'])
                refs['out'].play()
                refs['env'].start()
                refs['playing'] = True
                state.register(tag, refs['out'], playing=True)
                status_label.set_text(status_chip(True))

            ui.button('Trigger Envelope', on_click=trigger) \
                .props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, env=None, out=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')


# ═══════════════════════ Mixing demos ═══════════════════════


def fader_demo():
    tag = state.tag('fader')
    refs = {'source': None, 'fader': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Fader', 'Smooth amplitude fading')
        ui.separator()

        freq_slider = ui.slider(min=100, max=2000, value=440,
                                step=1).classes('w-full')
        ui.label().bind_text_from(freq_slider,
                                  'value',
                                  backward=lambda v: f'Frequency: {v} Hz')

        goal_slider = ui.slider(min=0, max=1.0, value=0.5, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(goal_slider,
                                  'value',
                                  backward=lambda v: f'Goal: {v:.2f}')

        dur_slider = ui.slider(min=0.01, max=5.0, value=1.0, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(
            dur_slider,
            'value',
            backward=lambda v: f'Fade duration: {v:.2f} s')

        mode_select = ui.toggle(
            {
                FADE_LINEAR: 'Linear',
                FADE_EXPONENTIAL: 'Exp',
                FADE_LOWPASS: 'Lowpass',
                FADE_SMOOTH: 'Smooth'
            },
            value=FADE_SMOOTH).classes('w-full')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['fader'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['source'] is None:
                        refs['source'] = Sine(freq_slider.value, 1.0)
                        refs['fader'] = Fader(refs['source'], 0.0,
                                              mode_select.value)
                        state.register(tag, refs['fader'])
                    refs['fader'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def apply_fade():
                if refs['fader'] and refs['playing']:
                    refs['fader'].set_dur(dur_slider.value)
                    refs['fader'].set_mode(mode_select.value)
                    refs['fader'].set_goal(goal_slider.value)

            ui.button('Apply Fade', on_click=apply_fade) \
                .props('color=accent')

            def cleanup():
                state.remove(tag)
                refs.update(source=None, fader=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')


def sum_demo():
    tag = state.tag('sum')
    refs = {'sources': [], 'mixer': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Sum', 'Additive mixer -- stack multiple sines')
        ui.separator()

        freq_input = ui.number('Frequency (Hz)', value=440, min=20, max=4000)
        amp_input = ui.number('Amplitude',
                              value=0.2,
                              min=0,
                              max=1.0,
                              step=0.05)
        source_list = ui.label('Sources: (none)')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def add_source():
                if not ensure_connected():
                    return
                if refs['mixer'] is None:
                    refs['mixer'] = Sum(1)
                    state.register(tag, refs['mixer'])
                s = Sine(freq_input.value, amp_input.value)
                refs['sources'].append(s)
                refs['mixer'].ins(s)
                source_list.set_text(
                    f"Sources: {len(refs['sources'])} sine(s)")

            ui.button('Add Sine', on_click=add_source).props('color=accent')

            def toggle():
                if not ensure_connected():
                    return
                if refs['mixer'] is None:
                    ui.notify('Add at least one source first', type='info')
                    return
                if refs['playing']:
                    refs['mixer'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    refs['mixer'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(sources=[], mixer=None, playing=False)
                source_list.set_text('Sources: (none)')
                status_label.set_text(status_chip(False))

            ui.button('Destroy All', on_click=cleanup) \
                .props('color=negative outline')


def fade_demo():
    tag = state.tag('fade')
    refs = {'source': None, 'playing': False}

    with ui.card().classes('w-full'):
        make_card_header('Fade In / Out',
                         'Demonstrate ugen.fade_in() and ugen.fade()')
        ui.separator()

        freq_slider = ui.slider(min=100, max=2000, value=440, step=1) \
            .classes('w-full')
        ui.label().bind_text_from(freq_slider,
                                  'value',
                                  backward=lambda v: f'Frequency: {v} Hz')

        dur_slider = ui.slider(min=0.1, max=5.0, value=1.0, step=0.1) \
            .classes('w-full')
        ui.label().bind_text_from(
            dur_slider,
            'value',
            backward=lambda v: f'Fade duration: {v:.1f} s')

        mode_select = ui.toggle(
            {
                FADE_LINEAR: 'Linear',
                FADE_EXPONENTIAL: 'Exp',
                FADE_LOWPASS: 'Lowpass',
                FADE_SMOOTH: 'Smooth'
            },
            value=FADE_SMOOTH).classes('w-full')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def do_fade_in():
                if not ensure_connected():
                    return
                if refs['source'] is None:
                    refs['source'] = Sine(freq_slider.value, 0.4)
                    state.register(tag, refs['source'])
                refs['source'].fade_in(dur_slider.value,
                                       mode=mode_select.value)
                refs['playing'] = True
                status_label.set_text(status_chip(True))
                ui.notify(f'Fading in over {dur_slider.value:.1f}s',
                          type='info')

            ui.button('Fade In', on_click=do_fade_in) \
                .props('color=primary')

            def do_fade_out():
                if refs['source'] is not None and refs['playing']:
                    refs['source'].fade(dur_slider.value,
                                        mode=mode_select.value)
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                    ui.notify(f'Fading out over {dur_slider.value:.1f}s',
                              type='info')

            ui.button('Fade Out', on_click=do_fade_out) \
                .props('color=secondary')

            def cleanup():
                if refs['source'] is not None:
                    try:
                        refs['source'].mute()
                    except Exception:
                        pass
                state.remove(tag)
                refs.update(source=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        ui.label('Fade In smoothly connects the source to audio output; '
                 'Fade Out smoothly disconnects it.') \
            .classes('text-xs text-gray-400 italic')


def mix_demo():
    tag = state.tag('mix')
    refs = {'sources': {}, 'mixer': None, 'playing': False, 'counter': 0}

    with ui.card().classes('w-full'):
        make_card_header('Mix',
                         'Named-input mixer with per-channel gain control')
        ui.separator()

        freq_input = ui.number('Frequency (Hz)', value=440, min=20, max=4000)
        gain_input = ui.number('Gain', value=0.3, min=0, max=1.0, step=0.05)
        source_list = ui.label('Inputs: (none)')
        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def add_source():
                if not ensure_connected():
                    return
                if refs['mixer'] is None:
                    refs['mixer'] = Mix(1)
                    state.register(tag, refs['mixer'])
                refs['counter'] += 1
                name = f's{refs["counter"]}'
                s = Sine(freq_input.value, 1.0)
                refs['sources'][name] = s
                refs['mixer'].ins(name, s, gain_input.value)
                source_list.set_text(
                    f"Inputs: {', '.join(refs['sources'].keys())}")

            ui.button('Add Sine', on_click=add_source).props('color=accent')

            def toggle():
                if not ensure_connected():
                    return
                if refs['mixer'] is None:
                    ui.notify('Add at least one input first', type='info')
                    return
                if refs['playing']:
                    refs['mixer'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    refs['mixer'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(sources={}, mixer=None, playing=False, counter=0)
                source_list.set_text('Inputs: (none)')
                status_label.set_text(status_chip(False))

            ui.button('Destroy All', on_click=cleanup) \
                .props('color=negative outline')

        ui.label('Unlike Sum, Mix supports named inputs and per-input '
                 'gain with smooth crossfading.') \
            .classes('text-xs text-gray-400 italic')


# ═══════════════════════ Math / Unary demos ═══════════════════════


def math_demo():
    tag = state.tag('math')
    refs = {'s1': None, 's2': None, 'math': None, 'playing': False}

    MATH_OPS = {
        'Multiply': arco.MATH_OP_MUL,
        'Add': arco.MATH_OP_ADD,
        'Subtract': arco.MATH_OP_SUB,
        'Divide': arco.MATH_OP_DIV,
        'Max': arco.MATH_OP_MAX,
        'Min': arco.MATH_OP_MIN,
        'Clip': arco.MATH_OP_CLP,
        'Power': arco.MATH_OP_POW,
        'Less': arco.MATH_OP_LT,
        'Greater': arco.MATH_OP_GT,
        'Soft Clip': arco.MATH_OP_SCP,
    }

    with ui.card().classes('w-full'):
        make_card_header('Math', 'Binary math operation on two audio signals')
        ui.separator()

        op_select = ui.select(list(MATH_OPS.keys()),
                              value='Multiply',
                              label='Operation').classes('w-full')

        freq1 = ui.slider(min=20, max=2000, value=440,
                          step=1).classes('w-full')
        ui.label().bind_text_from(freq1,
                                  'value',
                                  backward=lambda v: f'Source 1: {v} Hz')

        freq2 = ui.slider(min=0.5, max=2000, value=3,
                          step=0.5).classes('w-full')
        ui.label().bind_text_from(
            freq2, 'value', backward=lambda v: f'Source 2: {v} Hz (or LFO)')

        amp2 = ui.slider(min=0, max=1.0, value=0.5,
                         step=0.01).classes('w-full')
        ui.label().bind_text_from(amp2,
                                  'value',
                                  backward=lambda v: f'Source 2 amp: {v:.2f}')

        status_label = ui.label(status_chip(False))

        with ui.row().classes('items-center gap-4'):

            def toggle():
                if not ensure_connected():
                    return
                if refs['playing']:
                    refs['math'].mute()
                    refs['playing'] = False
                    status_label.set_text(status_chip(False))
                else:
                    if refs['s1'] is None:
                        refs['s1'] = Sine(freq1.value, 0.5)
                        refs['s2'] = Sine(freq2.value, amp2.value)
                        op = MATH_OPS[op_select.value]
                        refs['math'] = Math(op, refs['s1'], refs['s2'])
                        state.register(tag, refs['math'])
                    refs['math'].play()
                    refs['playing'] = True
                    status_label.set_text(status_chip(True))

            ui.button('Play / Stop', on_click=toggle).props('color=primary')

            def cleanup():
                state.remove(tag)
                refs.update(s1=None, s2=None, math=None, playing=False)
                status_label.set_text(status_chip(False))

            ui.button('Destroy', on_click=cleanup) \
                .props('color=negative outline')

        ui.label('Tip: For ring modulation, use Multiply with a low-freq '
                 'Source 2.').classes('text-xs text-gray-400 italic')


# ═══════════════════════ Utility demos ═══════════════════════


def panning_demo():
    with ui.card().classes('w-full'):
        make_card_header('Panning Laws',
                         'Compare linear, equal-power, and -4.5 dB')
        ui.separator()

        pan_slider = ui.slider(min=0, max=1.0, value=0.5, step=0.01) \
            .classes('w-full')
        ui.label().bind_text_from(
            pan_slider, 'value', backward=lambda v: f'Pan: {v:.2f} (0=L, 1=R)')

        result_label = ui.label('')

        def update_display(e=None):
            x = pan_slider.value
            lin = pan_linear(x)
            ep = pan_eqlpow(x)
            p45 = pan_45(x)
            result_label.set_text(
                f'Linear: L={lin[0]:.3f}  R={lin[1]:.3f}  |  '
                f'EqPow: L={ep[0]:.3f}  R={ep[1]:.3f}  |  '
                f'-4.5dB: L={p45[0]:.3f}  R={p45[1]:.3f}')

        pan_slider.on('update:model-value', update_display)
        update_display()


def pitch_conversion_demo():
    with ui.card().classes('w-full'):
        make_card_header('Pitch Conversion', 'Hz / MIDI step / ratio')
        ui.separator()

        step_slider = ui.slider(min=21, max=108, value=69, step=0.5) \
            .classes('w-full')
        ui.label().bind_text_from(step_slider,
                                  'value',
                                  backward=lambda v: f'MIDI step: {v}')

        result_label = ui.label('')

        def update(e=None):
            s = step_slider.value
            hz = step_to_hz(s)
            note_names = [
                'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'
            ]
            octave = int(s) // 12 - 1
            name = note_names[int(s) % 12]
            result_label.set_text(
                f'{name}{octave}  |  {hz:.2f} Hz  |  '
                f'step_to_hz({s}) = {hz:.2f}  |  '
                f'hz_to_step({hz:.1f}) = {hz_to_step(hz):.2f}')

        step_slider.on('update:model-value', update)
        update()


def velocity_demo():
    with ui.card().classes('w-full'):
        make_card_header('Velocity Conversion', 'MIDI velocity / linear / dB')
        ui.separator()

        vel_slider = ui.slider(min=1, max=127, value=100, step=1) \
            .classes('w-full')
        ui.label().bind_text_from(vel_slider,
                                  'value',
                                  backward=lambda v: f'MIDI velocity: {v}')

        result_label = ui.label('')

        def update(e=None):
            v = vel_slider.value
            lin = vel_to_linear(v)
            db = arco.vel_to_db(v)
            result_label.set_text(
                f'vel={v}  ->  linear={lin:.4f}  ->  dB={db:.2f}')

        vel_slider.on('update:model-value', update)
        update()


# ═══════════════════════ Page layout ═══════════════════════


def build_page():
    ui.colors(primary='#1976D2', secondary='#424242', accent='#82B1FF')

    with ui.header().classes('items-center justify-between bg-primary'):
        ui.label('Arco Interactive Demo').classes(
            'text-h5 text-white font-bold')
        with ui.row().classes('items-center gap-4'):
            conn_label = ui.label('Disconnected').classes('text-white text-sm')

            async def do_connect():
                conn_label.set_text('Connecting...')
                state._connect_error = None
                await run.io_bound(state.connect)
                if state.connected:
                    conn_label.set_text('Connected')
                    ui.notify('Connected to Arco server', type='positive')
                elif state._connect_error:
                    conn_label.set_text('Disconnected')
                    ui.notify(f'Connection failed: {state._connect_error}',
                              type='negative')
                else:
                    conn_label.set_text('Disconnected')

            ui.button('Connect', on_click=do_connect) \
                .props('color=white text-color=primary dense')

    with ui.left_drawer(value=True).classes('bg-grey-2') as drawer:
        ui.label('Categories').classes('text-subtitle1 font-bold q-mb-sm')
        tabs = ui.tabs().props('vertical').classes('w-full')
        with tabs:
            osc_tab = ui.tab('Oscillators', icon='graphic_eq')
            filt_tab = ui.tab('Filters', icon='tune')
            fx_tab = ui.tab('Effects', icon='auto_fix_high')
            env_tab = ui.tab('Envelopes', icon='show_chart')
            mix_tab = ui.tab('Mixing', icon='merge_type')
            math_tab = ui.tab('Math', icon='calculate')
            util_tab = ui.tab('Utilities', icon='build')

    with ui.tab_panels(tabs, value=osc_tab) \
            .classes('w-full max-w-4xl mx-auto q-pa-md'):

        with ui.tab_panel(osc_tab):
            ui.label('Oscillators').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                sine_demo()
                tableosc_demo()

        with ui.tab_panel(filt_tab):
            ui.label('Filters').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                lowpass_demo()
                reson_demo()
                allpass_demo()

        with ui.tab_panel(fx_tab):
            ui.label('Effects').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                delay_demo()
                blend_demo()
                pitch_shift_demo()
                granstream_demo()

        with ui.tab_panel(env_tab):
            ui.label('Envelopes').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                pwl_demo()

        with ui.tab_panel(mix_tab):
            ui.label('Mixing').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                fader_demo()
                fade_demo()
                sum_demo()
                mix_demo()

        with ui.tab_panel(math_tab):
            ui.label('Math Operations').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                math_demo()

        with ui.tab_panel(util_tab):
            ui.label('Utilities (no audio)').classes('text-h6 q-mb-md')
            with ui.column().classes('gap-4 w-full'):
                panning_demo()
                pitch_conversion_demo()
                velocity_demo()

    with ui.footer().classes('bg-grey-3 text-grey-8 text-xs'):
        ui.label('Arco Sound Synthesis Engine -- Python Interface Demo')


# ═══════════════════════ Entry point ═══════════════════════

build_page()
ui.run(title='Arco Demo', port=8080, reload=False)
