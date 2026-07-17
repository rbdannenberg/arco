from arco_ugens import *

class Onset(Ugen):

    def __init__(self, input, reply_addr):
        super().__init__(new_ugen_id(), "Onset", 0, A_RATE, "Us",
                         None, True, 'input', input, 'reply_addr', reply_addr)



def onset(input, reply_addr):
    return Onset(input, reply_adder)

