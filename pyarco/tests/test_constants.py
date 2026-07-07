import arco_engine
import arco_instr


def test_action_constants_single_sourced():
    assert arco_instr.ACTION_TERM == arco_engine.ACTION_TERM == 1
    assert arco_instr.ACTION_END == arco_engine.ACTION_END == 16
    assert arco_instr.ACTION_REM == arco_engine.ACTION_REM == 32
    assert (arco_instr.ACTION_END_OR_TERM
            == arco_engine.ACTION_END_OR_TERM == 17)


def test_mute_finish_single_sourced():
    assert arco_instr.MUTE is arco_engine.MUTE
    assert arco_instr.FINISH is arco_engine.FINISH
