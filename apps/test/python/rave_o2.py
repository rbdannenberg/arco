from o2litepy import O2lite, O2blob
import time
import struct
import torch

from absl import flags, app
import rave_utils


ENSEMBLE = "arco"
BL = 32

FLAGS = flags.FLAGS
flags.DEFINE_string('model', required=True, default=None, help="model path")

# initialize globals:
enabled = False
last_hello_time = -1
frame_count = 0
start_time = 0
start_frame = 0
o2audioio_id = -1
inchans = 0
outchans = 0
sampletype = 0
samplerate = 44100.0
prepped = False

device = torch.device('cpu')
model = None


def blob_get_int16(blob):
    return struct.unpack(f'>{blob.size // 2}h', blob.data)

def blob_get_float(blob):
    return struct.unpack(f'>{blob.size // 4}f', blob.data)

def blob_put_int16(blob, x):
    blob.data = struct.pack(f'>{len(x)}h', *x)
    blob.size = len(blob.data)

def blob_put_float(blob, x):
    blob.data = struct.pack(f'>{len(x)}f', *x)
    blob.size = len(blob.data)

            
def prep_handler(address, types, info):
    global inchans, outchans, samplerate, sampletype, \
           o2audioio_id, prepped
    o2audioio_id = o2lite.get_int32()
    inchans = o2lite.get_int32()
    outchans = o2lite.get_int32()
    samplerate = o2lite.get_float()
    sampletype = o2lite.get_int32()
    prepped = True
    print("Got prep message: id", o2audioio_id, "inchans", inchans, \
          "outchans", outchans,  "samplerate", samplerate, \
          "sampletype", "float" if sampletype == 1 else "16-bit")



def hello_handler(address, types, info):
    global o2audioio_id, prepped
    o2audioio_id = o2lite.get_int32()
    print("got hello: id", o2audioio_id)
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
        print("sending hello from enab_handler")
        return send_hello(id)
    if enab == enabled:
        return
    enabled = enab
    frame_count = frames
    start_time = timestamp
    start_frame = frames






last_report_time = 0  # secs since we printed max_process_time
max_process_time = 0  # max data_handler run time in seconds
max_poll_time = 0     # max o2lite.poll() run time in seconds

def data_handler(address, types, info):
    global max_process_time
    start = time.monotonic_ns()
    id = o2lite.get_int32()
    data_time = o2lite.get_time()
    frames = o2lite.get_int64()
    data = o2lite.get_blob()
    
    if not prepped or not enabled:
        print("sending hello from data_handler")
        return send_hello(id)
    if sampletype == 0: # sample is int
        samples = blob_get_int16(data)            
        # Normalize into [-1.0, 1.0]
        samples_float = [s * 3.0518509476e-5 for s in samples]
        samples_float = torch.tensor(samples_float)
        out_samples = rave_utils.process_audio(model, samples_float, samplerate, device).flatten()

        out_samples_int = (out_samples * 32767).round().to(torch.int16).tolist()
        
        # Validate output size matches input
        if len(out_samples_int) != len(samples):
            print(f"ERROR: Model output size mismatch!")
            print(f"  Input samples: {len(samples)}")
            print(f"  Output samples: {len(out_samples_int)}")
            print("  Model should not be changing audio length")
            
        blob_put_int16(data, out_samples_int)
    else:
        samples = blob_get_float(data)
        
        samples = torch.tensor(samples)
        out_samples = rave_utils.process_audio(model, samples, samplerate, device)
        
        out_samples_list = out_samples.flatten().tolist()
        
        if len(out_samples_list) != len(samples):
            print(f"ERROR: Model output size mismatch!")
            print(f"  Input samples: {len(samples)}")
            print(f"  Output samples: {len(out_samples_list)}")
            print("  Model should not be changing audio length")
            
        blob_put_float(data, out_samples_list)

    o2lite.send_cmd("/arco/o2aud/data", 0, "ithb", id, data_time, frames, data)
    # print("Sent data message: id", id, "frames", frames, \
    #       "data_len", data.size)
    end = time.monotonic_ns()
    max_process_time = max(max_process_time, (end - start) * 0.000000001)


def main(argv):
    # Load and set up rave model
    
    global o2lite, last_report_time, max_process_time, max_poll_time, model

    model = rave_utils.load_model(FLAGS.model, device)
    
    print(f"Starting RAVE model with device: {device}")
    print(f"Model loaded from: {FLAGS.model}")
    
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

    last_report_time = o2lite.local_time()
    blocked_time = 0
    while True:
        start = time.monotonic_ns()
        o2lite.poll()
        end = time.monotonic_ns()
        max_poll_time = max(max_poll_time, (end - start) * 0.000000001)
        now = o2lite.local_time()
        if now - last_report_time >= 3:
            print("max_process_time", max_process_time, "max_poll_time", \
                  max_poll_time, "(in seconds)")
            last_report_time = now
            max_process_time = 0
            max_poll_time = 0
        time.sleep(0.002)
            

if __name__ == "__main__":
    app.run(main)
