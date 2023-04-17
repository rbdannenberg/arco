/* fileio.h -- asynchronous file io system for Arco
 *
 * Roger B. Dannenberg
 * April 2023
 */

/*
Audio processing should be able to stream audio to or from disk, but
you cannot run blocking file io operations inside the audio thread.
We need to handle this in a separate thread. It could be done in the
"main" thread where O2 is running, but since that's also a pretty high
priority control thread, we are going to offload file io to another
thread.  We will use O2 to for communication because it will allow the
file io to run at lower priority without risking priority
inversion. The drawback is that the thread has to poll for messages,
but we will set the polling rate to a low value of 20 Hz, so the 
polling overhead will be very low. This will give a rather
high latency for file io requests, but if you are doing file io, you
should be using buffers and prefetching enough that you can tolerate a
lot of latency. We'll assume audio file reads and writes are in 8K
sample frames, or 167 msec at 48000 sample rate. The message has to
go through the O2 process to get to the file io thread, and it has to
get to the file io thread, so it has to wait for 4 polling loops. The
last wait is for the Arco thread to poll which we assume is short, so
that leaves at least 50 msec latency for the O2 and the file io thread.
This seems very safe. (There is no latency to read the file because we
assume as soon as a buffer is requested, file io can begin filling the
next one.) This is just a thought experiment -- all parameters
are adjustable, but I think these are doable and conservative.

API

This API is based on Aura's audio read/write unit generator, which was
paired with a file reader/writer object.  The key is to place all the
control on the unit generator side. We're going to do the
reading/converting on the reader side (no need to do this in a
real-time thread). 

Create the Ugen Strplay with filename, channels, start time, end
time, cycle flag, mix, expand:
    /arco/strplay/new "isiffBBB" id filename chans start end cycle
This means play from start to end, and if cycle is true continue
playing from start after you reach end. Output the number of
channels: if mix is true, extra file channels are mixed round-robin
to the available output channels. If expand is true, file channels
are "expanded" round-robin to fill the available output channels.
Otherwise, extra output channels are zero-filled and extra input
channels are discarded. This sends a request to the fileio service:

    /fileio/strplay/new "isffB" id filename start end cycle

When the file is opened, the first buffer is read and sent (see
/arco/strplay/samps) and then a ready message is sent:

    /arco/strplay/ready "iiB" id chans ready

If the file could not be opened, chans is 0. Otherwise chans is the
actual number of channels in the file. The number of channels returned
in Audioblocks will be this number, independent of the actual channels
output by the unit generator. If the start time requested is beyond
the end of file or there was some other error getting ready to stream
the file, ready will be false. The Arco side can still issue a "play"
message (the reply will be a buffer with no samples and last will be
set), and the Arco side should issue a "stop" message. If the file is
opened successfully, ready will be true.

A separate message starts or stops playing:

    /arco/strplay/play "iB" id play

When "play" is received to start playing, the second buffer is
read and sent (see /arco/strplay/samps).

As each buffer is (Audioblock) is output from the Strplay Ugen,
request to refill with the next block of samples from the file:

    /fileio/strplay/read "i" id

The reply with the block of samples is:

    /arco/strplay/samps "ih" id address

where address is a pointer to the data (this of course assumes shared
memory) and last is true if this is the last block of the file.

To end playback and close the file:

    /fileio/strplay/play "iB" id play (= false)

(It is acceptable to close the file and delete the reader object if
cycle is false and the last samples of the file have been sent. The
play message can simply be ignored because id will not match.)
*/

int fileio_initialize();

class Fileio_obj;

extern Vec<Fileio_obj *> fileio_objs;

class Fileio_obj {
public:
    int32_t id;

    Fileio_obj(int id_) { id = id_; };

    ~Fileio_obj() { 
        for (int i = 0; i < fileio_objs.size(); i++) {
            if (fileio_objs[i] == this) {
                fileio_objs.remove(i);
                break;
            }
        }
    }
};


