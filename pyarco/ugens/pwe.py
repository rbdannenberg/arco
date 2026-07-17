# pwe.py -- piece-wise linear envelope

from arco_ugens import *
from ugens.envelope import *

class Pwe(Envelope):

    def __init__(self, points):
        super().__init__("Pwe", "/arco/pwe/", A_RATE, points)


def pwe(*points, initial_value=None, start=True, lin=False):
    return envelope(Pwe(points), initial_value, start, lin)



class Pweb(Envelope):

    def __init__(self, points):
        super().__init__("Pweb", "/arco/pweb/", B_RATE, points)


def pweb(*points, initial_value=None, start=True, lin=False):
    return envelope(Pweb(points), initial_value, start, lin)
