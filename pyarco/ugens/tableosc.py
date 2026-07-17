from arco_ugens import *

class Wavetables(Ugen):

    def __init__(self, freq, amp, phase, chans, classname, rate):
        if chans is None:
            chans = max_chans(max_chans(1, freq), amp)
        super().__init__(new_ugen_id(), classname, chans, rate, "UUf",
                         None, None, 'freq', freq, 'amp', amp, 'phase', phase)
        self.address_prefix = f"/arco/{self.classname.lower()}/"  # e.g. "/arco/tableosc/"

    def create_table(self, index, tlen, data, method_name):
        params = [self.arco_ref(), index]
        type_str = "ii"

        if tlen is not None:  # omit length for time-domain data
            params.append(tlen)
            type_str += "i"

        # Add the float vector data
        params.extend(data)
        type_str += "f" * len(data)

        o2lite.send_cmd(f"{self.address_prefix}{method_name}", 0, type_str,
                        *params)
        return self

    def create_tas(self, index, tlen, ampspec):
        # Create table from amplitude spectrum
        return self.create_table(index, tlen, ampspec, "createtas")

    def create_tcs(self, index, tlen, spec):
        # Create table from complex spectrum (amplitude and phase pairs)
        return self.create_table(index, tlen, spec, "createtcs")

    def create_ttd(self, index, samps):
        # Create table from time-domain data (table length is len(samps))
        return self.create_table(index, None, samps, "createttd")

    def borrow(self, lender):
        o2lite.send_cmd(f"{self.address_prefix}borrow", 0, "ii", self.arco_ref(),
                        lender.id)
        return self

    def select(self, index):
        o2lite.send_cmd(f"{self.address_prefix}sel", 0, "ii", self.arco_ref(), index)
        return self


class Tableosc(Wavetables):

    def __init__(self, freq, amp, phase=0, chans=None):
        super().__init__(freq, amp, phase, chans, "Tableosc", A_RATE)


class Tableoscb(Wavetables):

    def __init__(self, freq, amp, phase=0, chans=None):
        # Check rate requirements for b-rate oscillators
        if not isinstance(freq, (int, float)) and freq.rate != 'b':
            print("ERROR: 'freq' input to Ugen 'tableoscb' must be block rate")
            return None
        if not isinstance(amp, (int, float)) and amp.rate != 'b':
            print("ERROR: 'amp' input to Ugen 'tableoscb' must be block rate")
            return None
        super().__init__(freq, amp, phase, chans, "Tableoscb", B_RATE)


def tableosc(freq, amp, optional chans, keyword phase):
    p = 0
    if isnumber(phase):
        p = phase
    elif phase:
        print("ERROR 'phase' input to Ugen 'tableosc' must be a number")
    return Tableosc(freq, amp, p, chans)


def tableoscb(freq, amp, optional chans, keyword phase):
    if not chans:
        chans = max_chans(max_chans(1, freq), amp)
    p = 0
    if isnumber(phase):
        p = phase
    elif phase:
        print("ERROR 'phase' input to Ugen 'tableoscb' must be a number")
    return Tableoscb(freq, amp, p, chans)
