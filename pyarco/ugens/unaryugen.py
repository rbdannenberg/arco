from arco_ugens import *

# unaryugen.py - constructor implementation for abs, neg, exp, log,
#     log10, log2, sqrt, step_to_hz, hz_to_step, vel_to_lin, lin_to_vel.
#     For completeness, you can create a Unary as well with ugen_unary.

UNARY_OP_ABS = 0
UNARY_OP_NEG = 1
UNARY_OP_EXP = 2
UNARY_OP_LOG = 3
UNARY_OP_LOG10 = 4
UNARY_OP_LOG2 = 5
UNARY_OP_SQRT = 6
UNARY_OP_STEP_TO_HZ = 7
UNARY_OP_HZ_TO_STEP = 8
UNARY_OP_VEL_TO_LINEAR = 9
UNARY_OP_LINEAR_TO_VEL = 10
UNARY_OP_DB_TO_LINEAR = 11
UNARY_OP_LINEAR_TO_DB = 12

class Unary(Ugen):

    def __init__(self, op, x1, chans):
        chans = max_chans(chans, x1)
        super().__init__(new_ugen_id(), "Unary", chans, A_RATE, "iU",
                         None, None, 'op', op, "i", 'x1', x1, "abc")

class Unaryb(Ugen):

    def __init__(self, op, x1, chans=None):
        if not isinstance(x1, (int, float)) and x1.rate != B_RATE:
            print("ERROR: 'x1' input to unaryb Ugen must be block rate, op", op)
            return
        chans = max_chans(chans, x1)
        super().__init__(new_ugen_id(), "Unaryb", chans, B_RATE, "iU",
                         None, None, 'op', op, "i", 'x1', x1, "bc")

def ugen_abs(x1, chans=None):
    return Unary(UNARY_OP_ABS, x1, chans)
def ugen_absb(x1, chans=None):
    return Unaryb(UNARY_OP_ABS, x1, chans)

def ugen_neg(x1, chans=None):
    return Unary(UNARY_OP_NEG, x1, chans)
def ugen_negb(x1, chans=None):
    return Unaryb(UNARY_OP_NEG, x1, chans)

def ugen_exp(x1, chans=None):
    return Unary(UNARY_OP_EXP, x1, chans)
def ugen_expb(x1, chans=None):
    return Unaryb(UNARY_OP_EXP, x1, chans)

def ugen_log(x1, chans=None):
    return Unary(UNARY_OP_LOG, x1, chans)
def ugen_logb(x1, chans=None):
    return Unaryb(UNARY_OP_LOG, x1, chans)

def ugen_log10(x1, chans=None):
    return Unary(UNARY_OP_LOG10, x1, chans)
def ugen_log10b(x1, chans=None):
    return Unaryb(UNARY_OP_LOG10, x1, chans)

def ugen_log2(x1, chans=None):
    return Unary(UNARY_OP_LOG2, x1, chans)
def ugen_log2b(x1, chans=None):
    return Unaryb(UNARY_OP_LOG2, x1, chans)

def ugen_sqrt(x1, chans=None):
    return Unary(UNARY_OP_SQRT, x1, chans)
def ugen_sqrtb(x1, chans=None):
    return Unaryb(UNARY_OP_SQRT, x1, chans)

def ugen_step_to_hz(x1, chans=None):
    return Unary(UNARY_OP_STEP_TO_HZ, x1, chans)
def ugen_step_to_hzb(x1, chans=None):
    return Unaryb(UNARY_OP_STEP_TO_HZ, x1, chans)

def ugen_hz_to_step(x1, chans=None):
    return Unary(UNARY_OP_HZ_TO_STEP, x1, chans)
def ugen_hz_to_stepb(x1, chans=None):
    return Unaryb(UNARY_OP_HZ_TO_STEP, x1, chans)

def ugen_vel_to_linear(x1, chans=None):
    return Unary(UNARY_OP_VEL_TO_LINEAR, x1, chans)
def ugen_vel_to_linearb(x1, chans=None):
    return Unaryb(UNARY_OP_VEL_TO_LINEAR, x1, chans)

def ugen_linear_to_vel(x1, chans=None):
    return Unary(UNARY_OP_LINEAR_TO_VEL, x1, chans)
def ugen_linear_to_velb(x1, chans=None):
    return Unaryb(UNARY_OP_LINEAR_TO_VEL, x1, chans)

def ugen_db_to_linear(x1, chans=None):
    return Unary(UNARY_OP_DB_TO_LINEAR, x1, chans)
def ugen_db_to_linearb(x1, chans=None):
    return Unaryb(UNARY_OP_DB_TO_LINEAR, x1, chans)

def ugen_linear_to_db(x1, chans=None):
    return Unary(UNARY_OP_LINEAR_TO_DB, x1, chans)
def ugen_linear_to_dbb(x1, chans=None):
    return Unaryb(UNARY_OP_LINEAR_TO_DB, x1, chans)







