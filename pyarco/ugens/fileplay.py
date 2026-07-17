from arco_ugens import *

# fileplay.py -- stream audio from file

def fileplay(filename, chans=2, start=0, end=0,
             cycle=False, mix=False, expand=False):
    return Fileplay(chans, filename, start, end cycle, mix, expand)


class Fileplay(Ugen):

    def __init__(self, filename, chans, start, end, cycle, mix, expand):
        super().__init__(new_ugen_id(), "Fileplay", chans, A_RATE,
                         "sffBBB", None, None, 'filename', filename, "s",
                         'start', start, "f",
                         'end', end, "f",
                         'cycle', cycle, "B"
                         'mix', mix, "B",
                         'expand', expand, "B")

    def start(self, play_flag=True):
        o2lite.send_cmd("/arco/fileplay/play", 0, "iB",
                        self.arco_ref(), play_flag)
        return self

    def stop(self):
        return self.start(False)
