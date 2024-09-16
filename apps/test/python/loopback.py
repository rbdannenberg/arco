# loopback.py -- test program to receive audio from o2audioio ugen
#                over o2 and return it
#
# Roger B. Dannenberg
# Aug, 2024
#
"""
Normal behavior: 
    Receive /audiotest/prep
    Receive /audiotest/enab (with enab = True)
    Repeat:
        Receive /audiotest/data with blob
        Send blob back to /arco/o2aud/data

Recovery behavior: On startup, o2audioio object may already be sending
(and dropping) /data messages. When we start and connect loopback.py,
we may receive a /data message without an initial /prep and /enab
message. Alternatively, o2audioio may have blocked sending /data because
no replies have been received (but it still sends /hello every 3 sec).
    Receive /audiotest/data or /audiotest/enab or /audiotest/hello
        If the last sent /hello message was more than 1 second ago,
            send to /arco/o2aud/hello
        Otherwise, ignore the message
    Receive /audiotest/prep -- resume Normal behavior
"""


from o2lite import O2lite, O2blob
import time

ENSEMBLE = "arco"
BL = 32

# initialize globals:
enabled = False
last_hello_time = -1
last_data_time = -1
frame_count = 0
start_time = 0
start_frame = 0
o2audioio_id = -1
inchans = 0
outchans = 0
sampletype = 0
samplerate = 44100.0
prepped = False


class Audioblob:
    def __init__(floattype, chans, frames):
        self.floattype = floattype
        self.chans = chans
        self.frames = frames
        self.next = 0
        if chans > 0:
            self.blob = O2blob(size=frames * chans * 2 * (floattype + 1))
        else:
            self.blob = None


            
def prep_handler(address, types, info):
    global inchans, outchans, samplerate, sampletype, \
           o2audioio_id, prepped
    o2audioio_id = o2lite.get_int32()
    inchans = o2lite.get_int32()
    outchans = o2lite.get_int32()
    samplerate = o2lite.get_float()
    sampletype = o2lite.get_int32()
    prepped = True
    print("Got prep message: id", id, "inchans", inchans, \
          "outchans", outchans,  "samplerate", samplerate, \
          "sampletype", "float" if sampletype == 1 else "16-bit")
    # ignoring info because we will just return whatever data we get



def hello_handler(address, types, info):
    global o2audioio_id, prepped
    o2audioio_id = o2lite.get_int32()
    send_hello(o2audioio_id)



def send_hello(id):
    global last_hello_time
    time = o2lite.time_get()
    if last_hello_time > time:  # time went backwards!
        last_hello_time = time  # implies host restarted
    if last_hello_time > time - 1:
        return  # already sent hello recently
    last_hello_time = time
    print("send_hello", id)
    o2lite.send_cmd("/arco/o2aud/hello", 0, "i", id)
    prepped = False
    enabled = False
    print("Sent hello message.")



def enab_handler(address, types, info):
    global enabled, frame_count, start_time, start_frame
    id = o2lite.get_int32()
    enab = o2lite.get_bool()
    timestamp = o2lite.get_time()
    frames = o2lite.get_int64()
    print("Got enab message: id", id, "enab", enab, "time", timestamp,
          "frame_count", frames)
    if not prepped:
        return send_hello(id)
    # ignoring info because we will just return whatever data we get
    if enab == enabled:
        return
    enabled = enab
    frame_count = frames
    start_time = timestamp
    start_frame = frames



def data_handler(address, types, info):
    global last_data_time
    last_data_time = o2lite.time_get()
    id = o2lite.get_int32()
    time = o2lite.get_time()
    frames = o2lite.get_int64()
    data = o2lite.get_blob()
    if not prepped or not enabled:
        return send_hello(id)
    # return data to sender.
    o2lite.send_cmd("/arco/o2aud/data", 0, "ithb", id, time, frames, data)
    # print("Got data message: id", id, "time", time, "frames", frames, \
    #       "data_len", data.size)



def main():
    global o2lite
    o2lite = O2lite()
    o2lite.initialize(ENSEMBLE, debug_flags="")
    o2lite.set_services("audtest")
    o2lite.method_new("/audtest/prep", "iiifi", True, prep_handler, None)
    o2lite.method_new("/audtest/enab", "iBth", True, enab_handler, None)
    o2lite.method_new("/audtest/data", "ithb", True, data_handler, None)
    o2lite.method_new("/audtest/hello", "i", True, hello_handler, None)

    # wait for connect and clock sync
    while o2lite.time_get() < 0:
        o2lite.sleep(1)
    print("Connected to ensemble", ENSEMBLE, "O2time", o2lite.time_get())

    blocked_time = 0
    while True:
        o2lite.poll()
        time.sleep(0.002)
            

main()
