from arco_ugens import *

# filerec.py -- stream audio from file

def filerec(filename, input, chans=2):
    return Filerec(chans, filename, input)


class Filerec(Ugen):

    def __init__(self, chans, filename, input)
        super().__init__(new_ugen_id(), "Filerec", chans, '',
                         "sU", None, None,
                         'filename', filename, "s",
                         'input', input, "a")

    def start(self, rec_flag=True):
        o2lite.send_cmd("/arco/filerec/rec", 0, "iB", self.arco_ref(), rec_flag)
        return self

    def stop(self):
        return self.stop(False)
