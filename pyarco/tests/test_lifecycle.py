import gc

from arco_instr import Instrument, instr_begin
from arco_ugens import Sine
from conftest import pool_free_count


def test_instrument_borrows_output_id_and_leaks_no_slot(engine):
    free_before = pool_free_count(engine)
    instr_begin()
    out = Sine(440, 0.5)
    instr = Instrument("TestInstr", out)
    assert instr.id == out.id
    assert instr.owns_id is False
    # Slots consumed: Sine + its two auto-wrapped Consts. Nothing more.
    assert free_before - pool_free_count(engine) == 3


def test_instrument_del_does_not_free_borrowed_id(engine):
    instr_begin()
    out = Sine(440, 0.5)
    instr = Instrument("TestInstr", out)
    shared_id = out.id
    del instr
    gc.collect()
    # The output ugen still owns the slot: it must remain occupied.
    assert engine.id_pool.array[shared_id] is None  # None == occupied


def test_id_num_ugens_are_not_registered(engine):
    from arco_engine import OUTPUT_ID
    from arco_ugens import Sum
    s = Sum(2, True, id_num=OUTPUT_ID)
    assert s.owns_id is False
    assert OUTPUT_ID not in engine._ugens


from arco_ugens import Const, Sum


def test_dropping_last_reference_frees_ugen_and_slot(engine):
    free_before = pool_free_count(engine)
    s = Sine(440, 0.5)
    sid = s.id
    del s
    gc.collect()
    assert ("/arco/free", "i", (sid,)) in engine.o2lite.messages
    assert pool_free_count(engine) == free_before  # sine + consts reclaimed


def test_container_membership_keeps_child_alive(engine):
    mixer = Sum(1)
    child = Sine(440, 0.1)
    cid = child.id
    mixer.ins(child)
    del child
    gc.collect()
    assert ("/arco/free", "i", (cid,)) not in engine.o2lite.messages
    assert engine._ugens[cid] is not None


def test_replacing_input_releases_old_const(engine):
    s = Sine(440, 0.5)
    old_id = s.inputs['freq'].id
    s.set('freq', Const(220))
    gc.collect()
    assert ("/arco/free", "i", (old_id,)) in engine.o2lite.messages


def test_close_frees_survivors_and_resets_pool(engine):
    # capture the transport first: close() sets engine.o2lite to None
    transport = engine.o2lite
    s = Sine(440, 0.5)
    sid = s.id
    engine.close()
    assert ("/arco/free", "i", (sid,)) in transport.messages
    assert s.engine is None
    assert engine.o2lite is None


def test_instrument_method_after_close_is_noop(engine):
    instr_begin()
    out = Sine(440, 0.5)
    instr = Instrument("TestInstr", out)
    engine.close()
    instr.mute()  # engine.o2lite is None -> send_cmd no-ops; must not raise


def test_instrument_gc_after_close_is_noop(engine):
    transport = engine.o2lite
    instr_begin()
    out = Sine(440, 0.5)
    instr = Instrument("TestInstr", out)
    engine.close()
    n_msgs = len(transport.messages)
    del instr
    gc.collect()
    assert len(transport.messages) == n_msgs  # borrowed id: GC sends nothing


def test_set_alternate_pins_alternate(engine):
    from arco_ugens import Thru
    thru = Thru(Sine(440, 0.1))
    alt = Sine(220, 0.1)
    aid = alt.id
    thru.set_alternate(alt)
    del alt
    gc.collect()
    assert ("/arco/free", "i", (aid,)) not in engine.o2lite.messages


def test_recplay_borrow_pins_lender(engine):
    from arco_ugens import Recplay
    src = Sine(440, 0.1)
    rp1 = Recplay(src)
    rp2 = Recplay(src)
    lid = rp1.id
    rp2.borrow(rp1)
    del rp1
    gc.collect()
    assert ("/arco/free", "i", (lid,)) not in engine.o2lite.messages


def test_tableosc_borrow_pins_lender(engine):
    from arco_ugens import Tableosc
    lender = Tableosc(440, 0.5)
    borrower = Tableosc(220, 0.5)
    lid = lender.id
    borrower.borrow(lender)
    del lender
    gc.collect()
    assert ("/arco/free", "i", (lid,)) not in engine.o2lite.messages
