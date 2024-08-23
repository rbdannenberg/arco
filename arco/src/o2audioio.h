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

Buffering and Flow Control:

When /arco/o2aud/enab ID 1 is received, output messages will start
immediately, filling a message and sending it, but Arco output will be
zero for buffsize + msgsize frames, and incoming audio will be queued
to minimize dropouts once the output to Arco of "real" samples from O2
messages starts. This is based on the idea that if the latency for
sending, processing, and returning audio is zero, the buffer will just
be full after buffsize + msgsize frames (the msgsize frames is added
because there is a delay of msgsize before the first message is even
sent).

There is a potential problem if the network and receiver cannot keep
up with sent O2 messages. Since Arco uses a shared memory interface,
there is no blocking and Arco will just keep allocating memory to send
audio, even if the main thread running O2 has a blocked TCP socket. To
prevent this, note that the buffer should never underflow. If it does,
there will be at least buffsize + msgsize frames in flight. At that
point, O2audioio should stop sending messages and instead just drop
them, keeping count. When the buffer begins to fill again, O2audioio
can resume sending and set the drop parameter to the number of dropped
frames. This will help the receiver keep track of the stream position
relative to real time.

The case of no Arco output is similar except there are no incoming O2
messages with audio data. This makes flow control harder, so the O2
process receiving from Arco must return /data messages, using the drop
parameter to acknowledge how many frames have been received (either
actual frames or "virtual" dropped frames). There is no requirement to
acknowledge every incoming message. For example, you could send one
/data message with drop count for every 5 incoming /data messages.
This saves bandwidth but would increase overall latency by 4 * msgsize
frames.

The case of no Arco input is also the same. The O2 process sending
audio to Arco should use start time in the /enab message + duration of
msgsize frames as the "ideal" O2 time to send the first message, and
it should never compute faster/earlier than the audio rate starting at
that start time to avoid possibly overflowing the buffer for incoming
audio. The sender should also check to make sure messages will not
block, and if so, drop messages in case the network is not delivering
audio messages fast enough to keep up. See more below about dropping
audio data. With O2lite, direct writes without checking whether the
message will block should be OK. However, if the O2lite process gets
substantially behind real time, it might want to drop some samples to
catch up to the time line.

If the incoming audio queue is ever empty when data is expected,
output zero, keeping track of how many samples were missing (a
multiple of BL). When audio is again available, drop as many frames as
were missing so that the output always corresponds to the input
delayed by buffsize + msgsize. As mentioned above, underflow will also
stop the sending of audio messages from Arco.

When the non-Arco message sender would block, it drops a message and
uses the drop count to tell the O2audioio Ugen that samples are
missing. (Thus, the drop count is useful in both directions.) When the
incoming /data message with a drop count arrives to the O2audioio
Ugen, then if the queue is empty and the O2audioio object has a
positive output_missing, we subtract the minimum of output_missing and
the drop count from both output_missing and the drop count.

Then, we insert as many zeros as are reported in the drop count.
Finally, we add the actual samples to the queue.

Output messages:

When the stream starts, a message is sent to <destaddr>/enab with the
id of this Ugen and start = 1. Then, as audio becomes available, audio
is sent to <destaddr>/data. When the stream stops, another message is
sent to <destaddr>/enab with the id of this Ugen and start = 0.

Input messages:

When the stream starts, the audio process we are connected to can
start sending audio, either after processing audio from an O2audioio
source, or simply by generating and sending audio. In the case of just
sending audio, there are currently no corrections for sample rate
discrepancies, and the sender must not overflow the buffer set by
buffsize. Arco generally provides the reference clock which is based
on samples, so O2 time and sample count should stay in synchrony.

Messages are sent to /arco/o2aud/data, and the first parameter names
the destination O2audioio Ugen object.

API
    /o2aud/enab "ii" id enab

If enab, start the stream, collecting input until msgsize samples are
accumulated, at which time a message is sent.  If not enab, stop both
input and output immediately. This will effectively truncate the last
buffsize + msgsize samples which are either in flight or received but
enqueued.

*/

extern const char *O2audioio_name;

void arco_o2audioio_init();

class O2audioio : public Ugen {
public:
    char *dest_data_addr;
    char *dest_enab_addr;
    Blockqueue buffer;
    O2blob_ptr out_blob;
    int out_blob_frames;  // how many frames are in out_blob so far?
    int input_missing;   // how many frames of input were dropped because
            // an O2 message would block. This number is reduced to zero
            // when we successfully send a message with a drop count.
    int output_missing;  // how many frames of output were missing and
            // still expected, and were replaced by outputting zeros.
    int64_t sent_frames;  // how many frames have been sent?
                          // (including drop counts)
    int64_t recv_frames;  // how many frames have been received?
                          // (including drop counts)
    int frames_per_message;  // how many frames in each O2 message?
    int buffer_frame_size;   // how many frames can be stored in the queue?
    bool floattype;  // 0 for int16, 1 for float
    int input_chans;
    Ugen_ptr input;
    int input_stride;
    bool has_input;
    bool has_output;
    bool running;

    const char *classname() { return O2audioio_name; }

    O2audioio(int32_t id,  int32_t recvchans, Ugen_ptr input,
              char *destaddr, int32_t destchans, int32_t buffsize,
              int32_t sampletype, int32_t msgsize) : Ugen(id, 'a', recvchans) {
    // Create the Ugen O2audioio, which sends input to a destination via O2
    //     and outputs audio that is received via O2
    // Parameters:
    //     id - the new id for the Ugen (as usual)
    //     input - the id for the input Ugen (may or may not be used)
    //     destaddr - base address for outgoing O2 messages with audio data.
    //     destchans - how many channels to send via O2 (0 for none)
    //     recvchans - how many channels to receive via O2 (0 for none)
    //                 (also the number of output channels for this Ugen)
    //     buffsize - size in frames to buffer incoming audio messages
    //     sampletype - 0 for int16 and 1 for float
    //     msgsize - size in frames of O2 audio messages
    //
    // destaddr is appended with "/data" to form an O2 address for audio
    // content. When the O2audioio Ugen is created, a message is sent to
    // "<destaddr>/prep" with fields ("iiifi") id (this Ugen's id),
    // destchans, recvchans, samplerate, and sampletype.
    //
    // destchans can be 0, indicating no input or sending.
    // recvchans can be 0, indicating no output or receiving.
    // buffsize and msgsize are rounded up to multiples of BL (32).
    //
        char prepaddr[64];
        int addrlen = strlen(destaddr);
        out_blob_frames = 0;
        input_missing = 0;
        output_missing = 0;
        floattype = (sampletype == 1);
        has_input = (destchans == 0);
        input_chans = destchans;

        msgsize = ((msgsize + BL - 1) / BL) * BL;  // round up
        frames_per_message = msgsize;
        // compute message payload in bytes
        int outmsgsize = frames_per_message * destchans * 2 * (sampletype + 1);
        int inmsgsize = frames_per_message * recvchans * 2 * (sampletype + 1);

        if (has_input) {  // accept input and send to <sendaddr>/data
            dest_data_addr = O2_MALLOCNT(addrlen + 6, char);  // "/data<EOS>"
            strcpy(dest_data_addr, destaddr);
            if (addrlen > 0 && dest_data_addr[addrlen - 1] == '/') {
                addrlen--;
            }
            strcpy(dest_data_addr + addrlen, "/data");

            dest_enab_addr = O2_MALLOCNT(addrlen + 6, char);  // "/enab<EOS>"
            strcpy(dest_enab_addr, destaddr);
            strcpy(dest_enab_addr + addrlen, "/enab");

            // add size field for O2blob:
            out_blob = O2_MALLOCNT(outmsgsize + sizeof(uint32_t), O2blob);
        } else {  // no input, no sending
            has_input = false;
            dest_data_addr = NULL;
            dest_enab_addr = NULL;
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
            buffer_frame_size = buffsize * msgsize;
        }

        // <destdir>/prep "iiifi" id destchans recvchans samplerate sampletype
        if (strlen(prepaddr) > addrlen - 6) {
            arco_print("O2audioio: destination address \"%s\" too long.\n",
                       destaddr);
        } else {
            strcpy(prepaddr, destaddr);
            strcpy(prepaddr + addrlen, "/prep");
            o2sm_send_start();
            o2sm_add_int32(id);
            o2sm_add_int32(destchans);
            o2sm_add_int32(recvchans);
            o2sm_add_float(AR);
            o2sm_add_int32(sampletype);
            o2sm_send_finish(0, prepaddr, true);
        }
    }

    ~O2audioio() { 
        if (dest_data_addr) O2_FREE(dest_data_addr);
        if (dest_enab_addr) O2_FREE(dest_enab_addr);
        if (out_blob)       O2_FREE(out_blob);
        // buffer is freed by destructor
    }

    
    void data(double when, int32_t drop, O2blob_ptr samps) {
    // handle incoming audio samples message
        if (drop > 0) {
            recv_frames += drop;
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
        assert(samps->size == frames_per_message * chans * 2 * (floattype + 1));
        recv_frames += frames_per_message;
        buffer.enqueue(samps->data);
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }


    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }


    void enable(int32_t enab) {
    // handle /arco/o2aud/enab message, which starts this Ugen
        if (running != enab) {
            running = enab;
            o2sm_send_start();
            o2sm_add_int32(id);
            o2sm_add_int32(enab);
            o2sm_send_finish(0, dest_enab_addr, true);
        }
    }
            
    
    void real_run() {
        if (has_input) {
            Sample_ptr input_samps = input->run(current_block);
            if (floattype) {
                for (int i = 0; i < input_chans; i++) {
                    memcpy(((float *) out_blob->data) + out_blob_frames,
                           input_samps, BLOCK_BYTES);
                    input_samps += input_stride;
                }
                out_blob_frames += BL;
                assert(out_blob_frames <= frames_per_message);
                if (out_blob_frames == frames_per_message) {  // send 'em
                    if (sent_frames > recv_frames + buffer_frame_size) {
                        // have not received what we sent for too long
                        input_missing += frames_per_message;
                    } else {
                        o2sm_send_start();
                        o2sm_add_int32(id);
                        o2sm_add_double(o2sm_time_get());
                        o2sm_add_int32(input_missing);
                        o2sm_add_blob(out_blob);
                        o2sm_send_finish(0, dest_data_addr, true);
                        sent_frames += frames_per_message + input_missing;
                        input_missing = 0;
                    }
                    out_blob_frames = 0;  // start refilling the message
                }
            }
        }
    }
};


