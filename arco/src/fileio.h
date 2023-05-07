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
sample frames, or 167 msec at 48000 sample rate. If the message
goes through the O2 process to get to the file io thread, and it has to
get to the file io thread, then it has to wait for 4 polling loops. The
last wait is for the Arco thread to poll which we assume is short, so
that leaves at least 50 msec latency for the O2 and the file io thread.
This seems very safe. (There is no latency to read the file because we
assume as soon as a buffer is requested, file io can begin filling the
next one.) This is just a thought experiment -- all parameters
are adjustable, but I think these are doable and conservative. Also,
our implementation bypasses the main thread and queue messages directly
between the audio and fileio threads.

API

This API is based on Aura's audio read/write unit generator, which was
paired with a file reader/writer object.  The key is to place all the
control on the unit generator side. We're going to do the
reading/converting on the reader side (no need to do this in a
real-time thread). 

Create the Ugen Fileplay with filename, channels, start time, end
time, cycle flag, mix, expand:
    /arco/fileplay/new "isiffBBB" id filename chans start end cycle
This means play from start to end, and if cycle is true continue
playing from start after you reach end. Output the number of
channels: if mix is true, extra file channels are mixed round-robin
to the available output channels. If expand is true, file channels
are "expanded" round-robin to fill the available output channels.
Otherwise, extra output channels are zero-filled and extra input
channels are discarded. This sends a request to the fileio service:

    /fileio/fileplay/new "hsffB" addr filename start end cycle

where addr is the address of the Fileplay instance. (It is tempting
to send id, but if the Fileplay is freed by the client, then the id
will no longer work. See "References and reference counting" in
fileplay.h.

When the file is opened, the first buffer is read and sent (see
/arco/fileplay/samps) and then a ready message is sent:

    /arco/fileplay/ready "hiB" addr chans ready

If the file could not be opened, chans is 0. Otherwise chans is the
actual number of channels in the file. The number of channels returned
in Audioblocks will be this number, independent of the actual channels
output by the unit generator. If the start time requested is beyond
the end of file or there was some other error getting ready to stream
the file, ready will be false.

IMPORTANT: When ready is false, the Fileio_obj is deleted and this
is the last message from the object.  No further messages can be sent.

A separate message starts or stops playing:

    /fileio/fileplay/play "iB" id play

When "play" is received to start playing, the second buffer is
read and sent (see /arco/fileplay/samps).

As each buffer is (Audioblock) is output from the Fileplay Ugen,
request to refill with the next block of samples from the file:

    /fileio/fileplay/read "h" addr

The reply with the block of samples is:

    /arco/fileplay/samps "hh" addr address

where address is a pointer to the data (this of course assumes shared
memory) and last is true if this is the last block of the file.

To end playback and close the file:

    /fileio/fileplay/play "hB" addr play (= false)

This can be asynchronous with respect to file reading, so it is
possible for Arco to then receive an /arco/fileplay/samps
message. However, eventually, a reply will be sent:

    /arco/fileplay/ready "hiB" addr chans ready (= false)

which indicates the Fileio_obj has been deleted and no further
messages will follow for addr.

To shut down the entire fileio bridge and thread:

    /fileio/quit ""

Writing
-------

To write a file, we use a similar protocol with Ugen Filerec:

    /fileio/filerec/new "hsi" addr filename chans

where addr is the address of the Filerec instance.

When the file is opened, a ready message is sent:

    /arco/filerec/ready "hiB" addr ready

If the file could not be opened, or if the start time requested is
beyond the end of file or there was some other error getting ready to
stream the file, ready will be false and Fileio_obj will be
deleted. If the file is opened successfully, ready will be true.

As each buffer (Audioblock) is prepared by the Filerec Ugen, it is
sent to be written (the Audioblock last flag is set if this is the
last block and the file can be closed.

    /fileio/filerec/write "hh" addr address

The reply to release the block of samples is:

    /arco/filerec/samps "h" addr

The blocks are "owned" by the Filerec object, which knows which block
is being freed. The last flag is preserved in the Audioblock, and if
set, the fileio object is deleted, the file is closed, and no further
messages for addr are sent.

There is no final /arco/filerec/ready with ready == false; instead
there is /arco/filerec/samps with the last flag set.

*/

int fileio_initialize();

class Fileio_obj;

extern Vec<Fileio_obj *> fileio_objs;

class Fileio_obj {
public:
    int64_t addr;

    Fileio_obj(int64_t addr_) { addr = addr_; };

    ~Fileio_obj() { 
        for (int i = 0; i < fileio_objs.size(); i++) {
            if (fileio_objs[i] == this) {
                fileio_objs.remove(i);
                break;
            }
        }
    }
};


