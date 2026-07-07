from arco_ugens import Sine
from conftest import addresses


def test_sine_creation_sends_new_message(engine):
    s = Sine(440, 0.5)
    addrs = addresses(engine)
    assert "/arco/sine/new" in addrs
    assert addrs.count("/arco/const/newn") == 2  # freq and amp auto-wrapped
    assert s.id >= engine.id_pool.start_id
    assert s.inputs["freq"].id != s.inputs["amp"].id
    assert s._addr_prefix == "/arco/sine"
