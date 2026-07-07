from arco_ugens import Smoothb
from conftest import addresses


def test_smoothb_constructs_without_error(engine):
    sm = Smoothb(0.5)
    assert sm.chans == 1
    assert "/arco/smoothb/newn" in addresses(engine)


def test_smoothb_newn_sends_cutoff_then_all_values(engine):
    sm = Smoothb(0.5, cutoff=7)
    msg = [m for m in engine.o2lite.messages
           if m[0] == "/arco/smoothb/newn"][-1]
    assert msg[1] == "iff"  # id, cutoff, x0 -- server needs argc > 2
    assert msg[2] == (sm.id, 7, 0.5)


def test_smoothb_multichannel(engine):
    sm = Smoothb([0.1, 0.2, 0.3], cutoff=10)
    assert sm.chans == 3
    msg = [m for m in engine.o2lite.messages
           if m[0] == "/arco/smoothb/newn"][-1]
    assert msg[1] == "iffff"
    assert msg[2] == (sm.id, 10, 0.1, 0.2, 0.3)


import time

import arco_ugens
from arco_engine import OUTPUT_ID
from arco_ugens import Add, Route, Sine, Stdistr, Sum


def test_sum_ins_tracks_members(engine):
    s = Sum(1)
    child = Sine(440, 0.1)
    s.ins(child)
    assert s.members[child.id] is child
    s.rem(child)
    assert child.id not in s.members


def test_sum_swap_tracks_members(engine):
    s = Sum(1)
    a, b = Sine(440, 0.1), Sine(660, 0.1)
    s.ins(a)
    s.swap(a, b)
    assert a.id not in s.members
    assert s.members[b.id] is b


def test_add_route_stdistr_track_members(engine):
    child = Sine(440, 0.1)
    add = Add(1)
    add.ins(child)
    assert add.members[child.id] is child
    route = Route(1)
    route.ins(child, 0, 0)
    assert route.members[child.id] is child
    route.reminput(child)
    assert child.id not in route.members
    st = Stdistr(4, 0.5)
    st.ins(2, child)
    assert st.members[2] is child
    st.rem(2)
    assert 2 not in st.members


def test_play_and_mute_track_engine_output(engine):
    engine.output = Sum(2, True, id_num=OUTPUT_ID)
    s = Sine(440, 0.1)
    s.play()
    assert engine.output.members[s.id] is s
    s.mute()
    assert s.id not in engine.output.members


def test_fade_swaps_membership_and_releases_fader(engine, monkeypatch):
    monkeypatch.setattr(arco_ugens, "_FADE_CLEANUP_MARGIN", 0.05)
    engine.output = Sum(2, True, id_num=OUTPUT_ID)
    s = Sine(440, 0.1)
    s.play()
    faded = s.fade(0.05)
    assert s.id not in engine.output.members
    assert engine.output.members[faded.id] is faded
    time.sleep(0.3)
    assert faded.id not in engine.output.members
    assert faded._server_freed is True


def test_fade_in_completion_swaps_source_back(engine, monkeypatch):
    monkeypatch.setattr(arco_ugens, "_FADE_CLEANUP_MARGIN", 0.05)
    engine.output = Sum(2, True, id_num=OUTPUT_ID)
    s = Sine(440, 0.1)
    s.fade_in(0.05)
    fader = engine.fade_in_lookup[s.id]
    assert engine.output.members[fader.id] is fader
    time.sleep(0.3)
    assert s.id not in engine.fade_in_lookup
    assert engine.output.members[s.id] is s
    assert fader.id not in engine.output.members


def test_double_fade_reuses_fader_and_sends_one_swap(engine, monkeypatch):
    monkeypatch.setattr(arco_ugens, "_FADE_CLEANUP_MARGIN", 0.05)
    engine.output = Sum(2, True, id_num=OUTPUT_ID)
    s = Sine(440, 0.1)
    s.play()
    f1 = s.fade(0.05)
    f2 = s.fade(0.05)
    assert f2 is f1
    swaps = [m for m in engine.o2lite.messages if m[0] == "/arco/sum/swap"]
    assert len(swaps) == 1
    assert list(engine.output.members) == [f1.id]
    time.sleep(0.3)
    assert s._fade_out is None
    assert f1.id not in engine.output.members


def test_mute_during_fade_removes_live_fader(engine, monkeypatch):
    monkeypatch.setattr(arco_ugens, "_FADE_CLEANUP_MARGIN", 0.05)
    engine.output = Sum(2, True, id_num=OUTPUT_ID)
    s = Sine(440, 0.1)
    s.play()
    fader = s.fade(0.05)
    s.mute()
    assert fader.id not in engine.output.members
    rems = [m for m in engine.o2lite.messages if m[0] == "/arco/sum/rem"]
    assert rems[-1][2] == (OUTPUT_ID, fader.id)
