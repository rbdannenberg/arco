import gc
import os
import sys

import pytest

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import arco_engine
from arco_engine import ArcoEngine


class FakeO2Lite:
    """Records every O2 message instead of sending it."""

    def __init__(self):
        self.messages = []  # (address, type_string, params) tuples

    def send_cmd(self, address, time, type_string, *params):
        self.messages.append((address, type_string, params))

    def poll(self):
        pass


def addresses(eng):
    """All O2 addresses sent through this engine's fake transport."""
    return [m[0] for m in eng.o2lite.messages]


def pool_free_count(eng):
    """Walk the UgenID free list and count available slots."""
    count, slot = 0, eng.id_pool.free_head
    while slot is not None:
        count += 1
        slot = eng.id_pool.array[slot]
    return count


@pytest.fixture
def make_engine():
    """Factory for offline engines wired to a FakeO2Lite transport."""
    engines = []

    def _make():
        eng = ArcoEngine()
        eng.o2lite = FakeO2Lite()
        arco_engine._active_engine = eng
        engines.append(eng)
        return eng

    yield _make
    # Detach engines so any surviving ugen's __del__ is a no-op.
    for eng in engines:
        for ugen in list(eng._ugens.values()):
            ugen.engine = None
        for attr in (eng.zero, eng.zerob, eng.input, eng.output):
            if attr is not None:
                attr.engine = None
    arco_engine._active_engine = None
    gc.collect()


@pytest.fixture
def engine(make_engine):
    return make_engine()
