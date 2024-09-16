# make_tone.py -- test program to see if we can make a sine tone from Python
#
# Roger B. Dannenberg
# Aug 2024

from o2lite import O2lite
import time
ENSEMBLE = "arco"
OUTPUT_ID = 3  # ugen for audio output (see arco.srp)



def main():
    global o2lite
    o2lite = O2lite()
    o2lite.initialize(ENSEMBLE, debug_flags="a")

    # wait for connect and clock sync
    while o2lite.time_get() < 0:
        o2lite.sleep(1)
    print("Connected to ensemble", ENSEMBLE, "O2time", o2lite.time_get())

    # make a a sine oscillator with id = 100 and connect it to audio output
    # inputs are freq and amp. Both will be constants with ids 101 and 102

    # args are id, initial value (float)
    o2lite.send_cmd("/arco/const/newn", 0, "if", 101, 1000.0)  # frequency
    o2lite.send_cmd("/arco/const/newn", 0, "if", 102, 0.1)  # amplitude

    # args are id, chans, freq id, amp id
    o2lite.send_cmd("/arco/sine/new", 0, "iiii", 100, 1, 101, 102)

    # connect to output (a Sum ugen), args are Sum id, id of ugen to "play"
    o2lite.send_cmd("/arco/sum/ins", 0, "ii", OUTPUT_ID, 100)

    while True:
        # no scheduler in o2lite, so we'll do a hack to change
        # frequency every second:
        o2lite.sleep(1)
        o2lite.send_cmd("/arco/const/setn", 0, "if", 101, 1250.0)
        o2lite.sleep(1)
        o2lite.send_cmd("/arco/const/setn", 0, "if", 101, 1000.0)

    # Note there is no cleanup, so when this program exits, there
    # will be an orphan sine tone still sounding
        

main()
