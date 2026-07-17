# pwl.py -- piece-wise linear envelope

from arco_ugens import *
from ugens.envelope import *

class Pwl(Envelope):

    def __init__(self, points):
        super().__init__("Pwl", "/arco/pwl/", A_RATE, points)


def pwl(*points, initial_value=None, start=True):
    return envelope(Pwl(points), initial_value, start)



class Pwlb(Envelope):

    def __init__(self, points):
        super().__init__("Pwlb", "/arco/pwlb/", B_RATE, points)


def pwlb(*points, initial_value=None, start=True):
    return envelope(Pwlb(points), initial_value, start)
