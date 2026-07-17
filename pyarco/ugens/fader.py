from arco_ugens import *

# fader.py - constructor implementation
#
# Roger B. Dannenberg
# May 2026

# Support for fade_in: In the simple case, where we faded in ugen,
# we could just swap in the ugen and swap out the fader using a
# scheduled call. But what if the user says to fade() before the
# fade_in() is complete? What we need to do is cancel the pending
# end-of-fade-in, and use the fader to fade to zero. We'll use a
# dictionary for keeping track of everything fading in, mapping
# the ugen being faded in to the fader. Either fade_in_complete
# will remove the fader, or fade() will remove the fader from
# the dictionary.

class Fader(Ugen):

    def __init__(self, input, current, dur=None, goal=None, chans=None):
        if chans is None:
            chans = max_chans(1, input)
        super().__init__(new_ugen_id(), "Fader", chans, A_RATE, "Uf",
                         None, None, 'input', input, 'current', current)
        if dur is not None:
            fader.set_dur(dur)
        if goal is not None:
            fader_set_goal(goal)


    def set_current(self, current, chan=None):
        if chan is not None:
            o2lite.send_cmd("/arco/fader/cur", 0, "iif", self.arco_ref(), chan,
                            current)
        else:
            for i in range(self.chans):
                o2lite.send_cmd(
                    "/arco/fader/cur", 0, "iif", self.arco_ref(), i,
                    current if isinstance(current,
                                          (int, float)) else current[i])
        return self


    def set_dur(self, dur):
        o2lite.send_cmd("/arco/fader/dur", 0, "if", self.arco_ref(), dur)
        return self


    def set_mode(self, mode):
        o2lite.send_cmd("/arco/fader/mode", 0, "ii", self.arco_ref(), mode)
        return self


    def set_goal(self, goal, chan=None):
        if chan is not None:
            o2lite.send_cmd("/arco/fader/goal", 0, "iif",
                            self.arco_ref(), chan, goal)
        else:
            for i in range(self.chans):
                o2lite.send_cmd(
                    "/arco/fader/goal", 0, "iif", self.arco_ref(), i,
                    goal if isinstance(goal, (int, float)) else goal[i])
        return self
