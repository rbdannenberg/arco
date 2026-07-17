from arco_ugens import *

# delayvi.py - constructor implementation

def delayvi(input, delay, maxdelay, optional chans):
    chans = max_chans(chans, input, delay, maxdelay)
    Ugen(new_ugen_id(), "Delayvi", chans, A_RATE, "UUf", None, None,
         'input', input, "a", 'delay', delay, "abc",
         'maxdelay', maxdelay, "f")

