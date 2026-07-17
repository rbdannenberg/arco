# mathugen.py - constructor implementation for mult, add, ugen_div, ugen_max,
#           ugen_min, ugen_clip, ugen_less, ugen_greater, ugen_soft_clip.
#           For completeness, you can create a Math as well with ugen_math.

MATH_OP_MUL = 0
MATH_OP_ADD = 1
MATH_OP_SUB = 2
MATH_OP_DIV = 3
MATH_OP_MAX = 4
MATH_OP_MIN = 5
MATH_OP_CLP = 6  # min(max(x, -y), y) i.e. clip if |x| > y
MATH_OP_POW = 7
MATH_OP_LT = 8
MATH_OP_GT = 9
MATH_OP_SCP = 10
MATH_OP_PWI = 11
MATH_OP_RND = 12
MATH_OP_SH = 13
MATH_OP_QNT = 14
MATH_OP_RLI = 15
MATH_OP_HZDIFF = 16
MATH_OP_TAN = 17
MATH_OP_ATAN2 = 18
MATH_OP_SIN = 19
MATH_OP_COS = 20

from arco_ugens import *

class Math(Ugen):

    def __init__(self, op, x1, x2, chans):
        chans = max_chans(chans, x1, x2)
        super().__init__(new_ugen_id(), "Math", chans, A_RATE, "iUU",
                         None, None, 'op', op, "i", 
                         'x1', x1, "abc", 'x2', x2, "abc")

    def rliset(self, x):
        o2lite.send_cmd("/arco/math/rliset", 0, "if", self.arco_ref(), x)
        return self

class Mathb(Ugen):

    def __init__(self, op, x1, x2, chans):
        if not isinstance(x1, (int, float)) and x1.rate != B_RATE:
            print("ERROR: 'x1' input to mathb Ugen must be block rate, op ",
                  op, "x1", x1)
            return
        elif not isinstance(x2, (int, float)) and x2.rate != B_RATE:
            print("ERROR: 'x2' input to mathb Ugen must be block rate, op ",
                  op, "x2", x2)
            return
        else:
            chans = max_chans(1, x1, x2)
            super().__init__(new_ugen_id(), "Mathb", chans, B_RATE,
                             "iUU", None, None, 'op', op, "i",
                             'x1', x1, "bc", 'x2', x2, "bc")

    def rliset(self, x):
        o2lite.send_cmd("/arco/mathb/rliset", 0, "if", self.arco_ref(), x)
        return self


def mult(x1, x2, chans=None, x2_init=None):
    if x2_init is not None:
        chans = max_chans(chans, x1, x2)
        return Ugen(new_ugen_id(), "Multx", chans, A_RATE, "UUf",
                    'x1', x1, "abc", 'x2', x2, "abc", 'init', x2_init, "f")
    else:
        return Math(MATH_OP_MUL, x1, x2, chans)

def multb(x1, x2, chans=None):
    return Mathb(MATH_OP_MUL, x1, x2, chans)


def add(x1, x2, chans=None):
    return Math(MATH_OP_ADD, x1, x2, chans)
def addb(x1, x2, chans=None):
    return Mathb(MATH_OP_ADD, x1, x2, chans)

def sub(x1, x2, chans=None):
    return Math(MATH_OP_SUB, x1, x2, chans)
def subb(x1, x2, chans=None):
    return Mathb(MATH_OP_SUB, x1, x2, chans)

def ugen_div(x1, x2, chans=None):
    return Math(MATH_OP_DIV, x1, x2, chans)
def ugen_divb(x1, x2, chans=None):
    return Mathb(MATH_OP_DIV, x1, x2, chans)

def ugen_max(x1, x2, chans=None):
    return Math(MATH_OP_MAX, x1, x2, chans)
def ugen_maxb(x1, x2, chans=None):
    return Mathb(MATH_OP_MAX, x1, x2, chans)

def ugen_min(x1, x2, chans=None):
    return Math(MATH_OP_MIN, x1, x2, chans)
def ugen_minb(x1, x2, chans=None):
    return Mathb(MATH_OP_MIN, x1, x2, chans)

def ugen_clip(x1, x2, chans=None):
    return Math(MATH_OP_CLP, x1, x2, chans)
def ugen_clipb(x1, x2, chans=None):
    return Mathb(MATH_OP_CLP, x1, x2, chans)

def ugen_pow(x1, x2, chans=None):
    return Math(MATH_OP_POW, x1, x2, chans)
def ugen_powb(x1, x2, chans=None):
    return Mathb(MATH_OP_POW, x1, x2, chans)

def ugen_less(x1, x2, chans=None):
    return Math(MATH_OP_LT, x1, x2, chans)
def ugen_lessb(x1, x2, chans=None):
    return Mathb(MATH_OP_LT, x1, x2, chans)

def ugen_greater(x1, x2, chans=None):
    return Math(MATH_OP_GT, x1, x2, chans)
def ugen_greaterb(x1, x2, chans=None):
    return Mathb(MATH_OP_GT, x1, x2, chans)

def ugen_soft_clip(x1, x2, chans=None):
    return Math(MATH_OP_SCP, x1, x2, chans)
def ugen_soft_clipb(x1, x2, chans=None):
    return Mathb(MATH_OP_SCP, x1, x2, chans)

def ugen_powi(x1, x2, chans=None):
    return Math(MATH_OP_PWI, x1, x2, chans)
def ugen_powib(x1, x2, chans=None):
    return Mathb(MATH_OP_PWI, x1, x2, chans)

def ugen_rand(x1, x2, chans=None):
    return Math(MATH_OP_RND, x1, x2, chans)
def ugen_randb(x1, x2, chans=None):
    return Mathb(MATH_OP_RND, x1, x2, chans)

def ugen_sample_hold(x1, x2, chans=None):
    return Math(MATH_OP_SH, x1, x2, chans)
def ugen_sample_holdb(x1, x2, chans=None):
    return Mathb(MATH_OP_SH, x1, x2, chans)

def ugen_quantize(x1, x2, chans=None):
    return Math(MATH_OP_QNT, x1, x2, chans)
def ugen_quantizeb(x1, x2, chans=None):
    return Mathb(MATH_OP_QNT, x1, x2, chans)

def ugen_rli(x1, x2, chans=None):
    return Math(MATH_OP_RLI, x1, x2, chans)
def ugen_rlib(x1, x2, chans=None):
    return Mathb(MATH_OP_RLI, x1, x2, chans)

def ugen_hzdiff(x1, x2, chans=None):
    return Math(MATH_OP_HZDIFF, x1, x2, chans)
def ugen_hzdiffb(x1, x2, chans=None):
    return Mathb(MATH_OP_HZDIFF, x1, x2, chans)

def ugen_tan(x1, x2, chans=None):
    return Math(MATH_OP_TAN, x1, x2, chans)
def ugen_tanb(x1, x2, chans=None):
    return Mathb(MATH_OP_TAN, x1, x2, chans)

def ugen_atan2(x1, x2, chans=None):
    return Math(MATH_OP_ATAN2, x1, x2, chans)
def ugen_atan2b(x1, x2, chans=None):
    return Mathb(MATH_OP_ATAN2, x1, x2, chans)

def ugen_sin(x1, x2, chans=None):
    return Math(MATH_OP_SIN, x1, x2, chans)
def ugen_sinb(x1, x2, chans=None):
    return Mathb(MATH_OP_SIN, x1, x2, chans)

def ugen_cos(x1, x2, chans=None):
    return Math(MATH_OP_COS, x1, x2, chans)
def ugen_cosb(x1, x2, chans=None):
    return Mathb(MATH_OP_COS, x1, x2, chans)

























