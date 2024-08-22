/* o2audioio.h -- audio I/O via O2 messages
 *
 * Roger B. Dannenberg
 * Aug 2024
 */

/*
This Ugen sends incoming audio via O2 to a designated source and
outputs audio received via O2 from designated service, which is
created when the Ugen is instantiated.

MESSAGES 

Audio is sent in blocks that are a multiple of BL (32) using an O2
blob type. We use blobs because O2 does not have support for 16-bit
ints, but we want the option of sending 16-bit samples to lower the
message length and bandwidth. Each message will contain multiple
fields:

    source (int32) -- the ugen id for the source of the output data;
        for incoming-to-arco messages, the id of the destination
        O2audioio ugen.
    time (double) -- O2 time associated with first sample in data
    drop count (int32) -- how many frames were dropped from the
        stream, must be a multiple of BL.
    samples (blob) (a blob contains a length in bytes)

API

Create the Ugen O2audioio with:
    input ugen,
    destination base address or empty string,
    number of channels to send,
    number of channels to receive (also number of Ugen
        output channels),
    buffer size in frames
    sample type (0 for 16-bit int, 1 for 32-bit float),
    size of messages (in frames)
The addresses are appended with "/data" to form addresses for audio
content. When the stream starts, an empty message is sent to the
address appended with "/start", and when the stream ends, an empty
message is sent to the address appended with "/stop".

    /arco/o2aud/new "iisisiiii" id input destaddr sendchans
            recvchans buffsize sampletype msgsize

Note that this Ugen expects inputs to have sendchans channels (because
it sends the input audio over O2), and this Ugen will have recvchans
output channels (it outputs what it receives).  The number of channels
to send can be 0, indicating no input/sending.  Both buffsize and
msgsize are rounded up to multiples of BL (32), and it is these
rounded values that are referred to in the following.

A message is sent when an O2audioio Ugen is created:
    <destdir>/prep "iifi id outchans inchans samplerate sampletype 
where id is the Arco id of O2audioio Ugen sending this message,
outchans is the number of channels that it will send (0 if no audio
will be sent), inchans is the number of channels expected in return (0
if no audio is expected), samplerate is the audio sample rate,
sampletype is 0 for 16-bit int or 1 for 32-bit float.

    /o2aud/play "ib" id play

If play, start the stream, collecting input until msgsize samples are
accumulated, at which time a message is sent.  If not play, stop both
input and output immediately. This will effectively truncate the last
buffsize + msgsize samples which are either in flight or received but
enqueued.

Buffering:

Output will start immediately, but it will be zero for buffsize +
msgsize frames and incoming audio will be queued (to minimize dropouts
once the output starts). If the incoming audio queue is ever empty
when data is expected, output zero, keeping track of how many samples
were missing (a multiple of BL). When audio is again available, drop
as many frames as were missing so that the output always corresponds
to the input delayed by buffsize + msgsize.

When output would block O2audioio drops the outgoing message but keeps
track of how many frames were dropped. The number of dropped frames is
sent as the drop count in the next message. The drop count accumulates
until a message is actually sent.

When the buffer underflows, O2audioio outputs a block of zeros and
keeps track of how many samples were missing. When data actually
arrives (and is late), the samples are deleted until input catches
up. This should prevent the buffer from overflowing in cases where a
kernel buffer gets backed up and suddenly releases a backlog of O2
messages.

Incoming /data messages with audio also have a drop count (a multiple
of BL). When the sender would block, it drops a message and uses the
drop count to tell the O2audioio Ugen that samples are
missing. Similarly, when an incoming /data mesasge arrives with a drop
count, then if the queue is empty and the O2audioio object has a
positive output_missing, we subtract the minimum of output_missing and
the drop count from both output_missing and the drop count.

Then, we insert as many zeros as are reported in the drop
count. Finally, we add the actual samples to the queue.

Output messages:

When the stream starts, a message is sent to <destaddr>/play with the
id of this Ugen and start = 1. Then, as audio becomes available, audio
is sent to <destaddr>/data. When the stream stops, another message is
sent to <destaddr>/play with the id of this Ugen and start = 0.

Input messages:

When the stream starts, the audio process we are connected to can
start sending audio, either after processing audio from an O2audioio
source, or simply by generating and sending audio. In the case of just
sending audio, there are currently no corrections for sample rate
discrepancies, and the sender must not overflow the buffer set by
buffsize. (Clearly, this needs better support, e.g. through clock
synchronization.)

Messages are sent to /arco/o2aud/data 

*/

void arco_o2audioio_init();

class O2audioio : public Ugen {
public:
    char *dest_data_addr;
    char *dest_play_addr;
    Blockqueue buffer;
    char *out_blob;
    int input_missing;   // how many frames of input were dropped because
            // an O2 message would block. This number is reduced to zero
            // when we successfully send a message with a drop count.
    int output_missing;  // how many frames of output were missing and
            // still expected, and were replaced by outputting zeros.
    int frames_per_message;  // how many frames in each O2 message?
    bool floattype;  // 0 for int16, 1 for float
    
    bool has_input;
    bool has_output;
    bool running;
    
    O2audioio(int32_t id,  Ugen_ptr input, char *destaddr, int32_t destchans,
              int32_t recvchans, int32_t buffsize, int32_t sampletype,
              int32_t msgsize) : Ugen(id, 'a', recvchans) {
        input_missing = 0;
        output_missing = 0;
        floattype = (sampletype == 1);
        has_input = (destchans == 0);
        if (has_input) {  // accept input and send to <sendaddr>/data
            had_input = true;
            dest_data_addr = O2_MALLOCNT(addrlen + 6, char);  // "/data<EOS>"
            strcpy(dest_data_addr, destaddr);
            if (addrlen > 0 && dest_data_addr[addrlen - 1] == '/') {
                addrlen--;
            }
            strcpy(dest_data_addr + addrlen, "/data");

            dest_play_addr = O2_MALLOCNT(addrlen + 6, char);  // "/play<EOS>"
            strcpy(dest_play_addr, destaddr);
            strcpy(dest_play_addr + addrlen, "/play");

            msgsize = ((msgsize + BL - 1) / BL) * BL;  // round up
            frames_per_message = msgsize * destchans;
            // compute message payload in bytes
            msgsize = frames_per_message * 2 * (sampletype + 1);
            // add size field for O2blob:
            out_blob = O2_MALLOCNT(msgsize + sizeof(uint32_t), char);
        } else {  // no input, no sending
            has_input = false;
            dest_data_addr = NULL;
            dest_play_addr = NULL;
            out_blob = NULL;
            frames_per_message = 0;
        }
        init_input(input);

        has_output = (recvchans > 0);
        if (has_output) {  // receive from r
            int blocksize = BL * recvchans * 2;  // int16 samples per message
            buffsize = (buffsize + msgsize - 1) / msgsize;  // round up
            // buffsize is now number of msgsize that should fit in queue
            buffer.init(blocksize, buffsize);
        }
    }

    ~O2audioio() { 
        if (dest_data_addr) O2_FREE(dest_data_addr);
        if (dest_play_addr) O2_FREE(dest_play_addr);
        if (out_blob)           O2_FREE(blob);
        // buffer is freed by destructor
    }

    
    void data(double when, int32_t drop, O2blob_ptr samps) {
    // handle incoming audio samples message
        if (drop > 0) {
            if (buffer.get_fifo_len() == 0 && output_missing > 0) {
                int n = MIN(drop, output_missing);
                drop -= n;
                output_missing -= n;
            }
        }
        // insert zeros to compensate for dropped samples:
        while (drop > 0) {
            buffer.enqueue_zeros();
            drop -= frames_per_message;
        }
        assert(drop == 0);
        // insert data from message
        buffer.enqueue(samps.data);
    }
            
};


