import gc

from arco_engine import ACTION_END, ACTION_END_OR_TERM, ACTION_FREE
from arco_ugens import Sine


def test_action_target_does_not_pin_ugen(engine):
    s = Sine(440, 0.5)
    sid = s.id
    engine.register_action(s, ACTION_END_OR_TERM, s, 'mute')
    del s
    gc.collect()
    assert engine._ugens.get(sid) is None


def test_handler_calls_live_target(engine):
    calls = []

    class Target:
        def mute(self, status):
            calls.append(status)

    s = Sine(440, 0.5)
    t = Target()
    engine.register_action(s, ACTION_END_OR_TERM, t, 'mute')
    engine.actl_act_handler(0, "/actl/act", "iii", s.action_id, ACTION_END, 0)
    assert calls == [ACTION_END]


def test_handler_prunes_dead_targets(engine):
    s = Sine(440, 0.5)
    engine.register_action(s, ACTION_END_OR_TERM, s, 'mute')
    aid = s.action_id
    del s
    gc.collect()
    engine.actl_act_handler(0, "/actl/act", "iii", aid, ACTION_END, 0)
    assert engine.action_dict[aid].ugen_actions == []


def test_action_free_removes_entry(engine):
    s = Sine(440, 0.5)
    engine.register_action(s, ACTION_END_OR_TERM, s, 'mute')
    aid = s.action_id
    engine.actl_act_handler(0, "/actl/act", "iii", aid, ACTION_FREE, 0)
    assert aid not in engine.action_dict


def test_handler_isolates_callback_exceptions(engine):
    calls = []

    class Boom:
        def mute(self, status):
            raise RuntimeError("callback failure")

    class Target:
        def mute(self, status):
            calls.append(status)

    s = Sine(440, 0.5)
    boom, t = Boom(), Target()
    engine.register_action(s, ACTION_END_OR_TERM, boom, 'mute')
    engine.register_action(s, ACTION_END_OR_TERM, t, 'mute')
    aid = s.action_id
    # must not raise, must still deliver to the healthy target
    engine.actl_act_handler(0, "/actl/act", "iii", aid, ACTION_END, 0)
    assert calls == [ACTION_END]
    # pruning reassignment still ran (both targets alive -> both retained)
    assert len(engine.action_dict[aid].ugen_actions) == 2
