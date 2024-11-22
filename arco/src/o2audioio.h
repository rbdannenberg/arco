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
zero for buffsize frames, and incoming audio will be queued
to minimize dropouts once the output to Arco of "real" samples from O2
messages starts. This is based on the idea that we want to fill the 
buffer as a cushion against latency to avoid underflow, but we do not
want overflow. The worst case for overflow is if the latency for
sending, processing, and returning audio is zero. The buffer will just
be full after buffsize frames have been sent and immediately received.
But now it will take another msgsize frames of input to fill a send
message, during which time we will empty msgsize frames from the buffer,
so we will be ready with space for msgsize frames when they are received.

There is a potential problem if the network and receiver cannot keep
up with sent O2 messages. Since Arco uses a shared memory interface,
there is no blocking and Arco will just keep allocating memory to send
audio, even if the main thread running O2 has a blocked TCP socket. To
prevent this, note that the buffer should never underflow. If it does,
there will be at least buffsize frames in flight. At that point,
O2audioio should stop sending messages and instead just drop
them. When the buffer begins to fill again, O2audioio can resume
sending. To stay synchronized, o2audioio sends a frame count (it is
not reset when sending stops and restarts), and the same frame count
is returned with processed samples.

The case of no Arco output is similar except there are no incoming O2
messages with audio data. This makes flow control harder, so the O2
process receiving from Arco must return /data messages, using the
frame count parameter to acknowledge that frames have been
received. There is no requirement to acknowledge every incoming
message. For example, you could send one /data message with frame
count for every 5 incoming /data messages.  This saves bandwidth but
would increase overall latency by 4 * msgsize frames since the buffer
will need additional space of 4 * msgsize to avoid overflow for the
same round-trip latency.

The case of no Arco input is also the same. The O2 process sending
audio to Arco should use start time in the /enab message + duration of
msgsize frames as the "ideal" O2 time to send the first message, and
it should never compute faster/earlier than the audio rate starting at
that start time to avoid possibly overflowing the buffer for incoming
audio. The sender should also check to make sure messages will not
block, and if so, drop messages in case the network is not delivering
audio messages fast enough to keep up. See more below about dropping
audio data. With O2lite, direct writes without checking whether the
message will block should be OK as the sender will simply block and
wait. However, if the O2lite process gets substantially behind real
time, it might want to drop some samples to catch up to the time line.

If the incoming audio queue is ever empty when data is expected,
output zero. When audio is again available, drop as many frames as
necessary so that the output always corresponds to the input
delayed by buffsize. As mentioned above, underflow will also
stop the sending of audio messages from Arco.

When the non-Arco message sender would block, it can drop the message
and use the frame count to tell the O2audioio Ugen that samples are
missing. (Thus, the frame count is useful in both directions.) When
the incoming /data message arrives to the O2audioio Ugen, O2audioio
should always make the output correspond to the input frame count
delayed by buffsize.  This may involve inserting zeros into the
buffer before incoming message data.

If the non-Arco process is restarted while a stream is enabled and
sending messages, O2 will reconnect and immediately begin receiving
them. If it received a /data message without first receiving a /prep
and /enab messages, it should send a /hello message. The response is:
stop sending, empty the buffer and output message, send a /prep and
/enab message as if starting afresh (although the frame count will
continue increasing) and then, as msgsize frames accumulate, send a
/data message.

Another potential problem is that if the non-Arco process quits for
long enough (which is likely), o2audioio will not receive any /data
messages and it will stop sending /data (see above). It is not
convenient for Arco to get notification when the destination service
becomes available again, so Arco will be waiting for a /hello message
that causes it to restart the stream, and the other process will be
waiting for a /data message and deadlock occurs. To solve this, when
o2audioio stops sending /data messages, it begins sending /hello
messages every 3 seconds. When the receiver receives /hello (which
contains the o2audioio id, it replies with /hello to restart the
stream.

Output messages:

    <destaddr>/prep "iiifi" id destchans recvchans samplerate sampletype
        is sent when o2audioio is created and also in response to a 
        /hello message. destchans refers to the number of O2audioio input
        channels, which is the number of channels sent from Arco to
        <destaddr>/data. sampletype is 0 for int16 and 1 for float.
    <destaddr>/enab "iBth" id enab timestamp framecount
        is sent when the stream starts (enab = 1) or stops (enab = 0).
        timestamp is the O2 time corresponding to framecount
    <destaddr>/data "ithb" id when framecount blob
        is sent as audio becomes available, in chunks of msgsize frames,
        where msgsize is a parameter to the O2audioio constructor. The
        receiver can compute msgsize from the blob size, the sampletype,
        and destchans.
    <destaddr>/hello "i" id
        is sent every 3 seconds while the stream is enabled but blocked.
        The response (if there is a receiver) should be an identical
        /hello message to restart the stream.

Messages to O2audioio objects:

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
    /arco/o2aud/new "iiisiiii" id recvchans input destaddr destchans
                               buffsize sampletype msgsize
    /arco/o2aud/repl_input "ii" id input
    /arco/o2aud/enab "iB" id enab
        Note that when the stream stops, buffers are cleared and
        output is stopped immediately, which will truncate the last
        buffersize frames.
    /arco/o2aud/data "ithb" id when framecount sampleblob
        is sent only from the remote audio source. framecount matches
        the frame count of the input samples that were processed so it
        will normally be greater than the current O2audioio object's 
        input frame count. when is the actual O2time when the message
        was sent.
    /arco/o2aud/hello "i" id
        is sent only from the remote audio source when a /data message
        or /hello message is received before a /prep or /enab message.
        It instructs the O2audioio object to reset buffers, resend
        /prep and /enab, and resume sending /data messages (unless
        sending has been stopped, in which case /prep is sent followed
        by /enab with enab = false).
*/

extern const char *O2audioio_name;

void arco_o2audioio_init();

class O2audioio : public Ugen {
public:
    char *dest_addr_base;
    int dest_addr_base_len;
    Blockqueue buffer;
    Audioblob out_blob;
    int64_t frame_count;  // how many frames have been input? initially 0.

    // blocked_state is initially 0 and updated as follows:
    // 0->1 when buffer is empty -- /hello is sent to get a fast restart
    //     and restart_frame_count is set to frame_count;
    // 1->0 when 2 sec have elapsed since restart_frame_count;
    // 1->2 when buffer is empty within 3 sec after
    //     restart_frame_count -- /hello is sent and restart_frame_count
    //     is set to frame_count;
    // 2->0 when 3 sec have elapsed after restart_frame_count -- /hello
    //     is sent and restart_frame_count is set to 0.
    int blocked_state;
    int64_t restart_frame_count;  // while blocked, send /hello every 3 sec
            // using frame_count - blocked_frame_count as time elapsed
    int buffer_frames_max;   // how many frames can be stored in the queue?

    // frame count of the next available space in the buffer.
    // Invariant: The buffer stores buffer_frames_max frames, so when
    // the buffer is full, the next_buffer_frame is frame_count +
    // buffer_frames_max. As the buffer drains out, the
    // next_buffer_frame remains the same while frame_count increases
    // and buffer.get_fifo_len * BL (the number of frames in the
    // buffer) decreases at the same rate. So:
    //     next_buffer_frame = frame_count + buffer.get_fifo_len() * BL
    // We could simply compute next_buffer_frame using the invariant,
    // but it has been helpful for debugging to compute it separately
    // by incrementing each time we insert something into the
    // buffer. Then, we can test the invariant to make sure everything
    // is consistent.
    int64_t next_buffer_frame;

    bool floattype;  // 0 for int16, 1 for float
    int input_chans;
    Ugen_ptr input;
    int input_stride;
    bool has_input;
    bool has_output;
    bool running;

    // Keep track of minimum input buffer content
    int min_buffer_len;
    int64_t last_report_frame_count;


    const char *classname() { return O2audioio_name; }

    const char *complete_address(const char *method_name) {
    // append method_name to destination base address and return the
    // completed O2 address. This string is only valid until the next
    // call to complete_address().
        strcpy(dest_addr_base + dest_addr_base_len, method_name);
        return dest_addr_base;
    }

    void send_prep() {
    // <destdir>/prep "iiifi" id destchans recvchans samplerate sampletype
        const char *prepaddr = complete_address("prep");
        o2sm_send_start();
        o2sm_add_int32(id);
        o2sm_add_int32(input_chans);
        o2sm_add_int32(chans);
        o2sm_add_float(AR);
        o2sm_add_int32(floattype);
        o2sm_send_finish(0, prepaddr, true);
    }
    

    O2audioio(int32_t id,  int32_t recvchans, Ugen_ptr input,
              char *destaddr, int32_t destchans, int32_t buffsize,
              int32_t sampletype, int32_t msgsize) :
        out_blob(sampletype, destchans, ((msgsize + BL - 1) / BL) * BL),
        Ugen(id, 'a', recvchans) {
    // Create the Ugen O2audioio, which sends input to a destination via O2
    //     and outputs audio that is received via O2
    // Parameters:
    //     id - the new id for the Ugen (as usual)
    //     recvchans - how many channels to receive via O2 (0 for none)
    //                 (also the number of output channels for this Ugen)
    //     input - the id for the input Ugen (may or may not be used)
    //     destaddr - base address for outgoing O2 messages with audio data.
    //     destchans - how many channels to send via O2 (0 for none)
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
        blocked_state = 0;  // not blocked yet
        restart_frame_count = 0;  // when blocked, set this to frame_count

        frame_count = 0;
        floattype = (sampletype == 1);
        has_input = (destchans > 0);
        has_output = (recvchans > 0);
        input_chans = destchans;

        dest_addr_base_len = (int) strlen(destaddr);
        dest_addr_base = O2_MALLOCNT(dest_addr_base_len + 8, char);
        strcpy(dest_addr_base, destaddr);
        if (dest_addr_base_len > 0 &&
            dest_addr_base[dest_addr_base_len - 1] != '/') {
            dest_addr_base[dest_addr_base_len++] = '/';
            dest_addr_base[dest_addr_base_len] = 0;  // EOS
        }
        init_input(input);

        min_buffer_len = 0;  // give it an initial value even if unused
        last_report_frame_count = 0;

        if (has_output) {  // receive from r
            // blocksize is size in bytes of BL frames (of int16 or float):
            int blocksize = BL * recvchans * 2 * (sampletype + 1);
            // buffsize = size in frames to buffer incoming audio messages
            buffsize = (buffsize + BL - 1) / BL;  // round up
            // buffsize is now number of blocks (each block is BL frames)
            // that should fit in queue
            buffer.init(blocksize, buffsize, true);
            assert(buffer.get_fifo_len() == buffsize);
            buffer_frames_max = buffsize * BL;
            next_buffer_frame = buffer_frames_max;
            min_buffer_len = buffer_frames_max;
        }

        send_prep();
            
        printf("o2audioio created@%p id %d refcnt %d\n", this, id, refcount);
    }

    ~O2audioio() {
        printf("o2audioio destroy@%p id %d refcnt %d\n", this, id, refcount);
        if (dest_addr_base) O2_FREE(dest_addr_base);
        // buffer is freed by destructor
    }

    
    void data(double when, int64_t framecount, O2blob_ptr samps) {
    // handle incoming audio samples message
        assert(next_buffer_frame ==
               frame_count + buffer.get_fifo_len() * BL);
        double now = o2sm_time_get();
        int inq = buffer.get_fifo_len() * BL;
        min_buffer_len = MIN(min_buffer_len, inq);
        // test if contiguous with what is in queue:
        if (framecount + buffer_frames_max < next_buffer_frame) {
            return;  // this message is late, throw it out
        }
        while (framecount + buffer_frames_max > next_buffer_frame) {
            // some messages must have been dropped so we need to zero fill
            buffer.enqueue_zeros();
            next_buffer_frame += BL;
            assert(next_buffer_frame ==
                   frame_count + buffer.get_fifo_len() * BL);
        }
        assert(framecount + buffer_frames_max == next_buffer_frame);

        // insert data from message
        assert(samps->size == out_blob.frames * chans * 2 * (floattype + 1));
        // fix byte order if necessary
        blob_byteswap(samps->data, out_blob.frames, chans, floattype);
        assert(buffer.size() - buffer.get_fifo_len() >=
               out_blob.frames / BL);
        for (int frames = 0; frames < out_blob.frames; frames += BL) {
            buffer.enqueue(samps->data + frames * chans * 2 * (floattype + 1));
            next_buffer_frame += BL;
        }

        if (last_report_frame_count + AR * 3 <= frame_count) {
            arco_print("O2audioio %d min-buf-frms %d (%gs) "
                "max-lat %gs max-rnd-trip %gs\n", id, min_buffer_len,
                min_buffer_len * AP,
                (buffer_frames_max - min_buffer_len) * AP,
                (buffer_frames_max - out_blob.frames - min_buffer_len) * AP);
            min_buffer_len = buffer_frames_max;
            last_report_frame_count = frame_count;
        }
        assert(next_buffer_frame ==
               frame_count + buffer.get_fifo_len() * BL);
    }


    void send_hello() {
        printf("Sending /hello message\n");
        const char *data_addr = complete_address("hello");
        o2sm_send_start();
        o2sm_add_int32(id);
        o2sm_send_finish(0, data_addr, true);
    }
    
    
    void hello() {
    // handle hello message by resending prep and enab messages
        out_blob.next = 0;  // empty output message
        // empty buffer
        send_prep();
        if (running) {
            send_enab();
        }
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, &input_stride); }


    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }


    void send_enab() {
        if (running) {
            buffer.zero_fill(buffer.size());
            next_buffer_frame = frame_count + buffer_frames_max;
            printf("send_enab: fifo_len %d frame_count %lld "
                   "next_buffer_frame %lld\n", buffer.get_fifo_len(),
                   frame_count, next_buffer_frame);
            assert(buffer.get_fifo_len() * buffer.blocksize /* bytes */ ==
                   buffer_frames_max * 2 * (floattype + 1)  /* bytes */ );
            assert(next_buffer_frame ==
                   frame_count + buffer.get_fifo_len() * BL);
        }
        const char *enab_addr = complete_address("enab");
        o2sm_send_start();
        o2sm_add_int32(id);
        o2sm_add_bool(running);
        o2sm_add_time(o2sm_time_get());
        o2sm_add_int64(frame_count);
        o2sm_send_finish(0, enab_addr, true);
    }


    void enable(int32_t enab) {
    // handle /arco/o2aud/enab message, which starts this Ugen
        if (running != enab) {
            running = enab;
            send_enab();
        }
    }
            
    
    void real_run() {
        if (has_input) {
            Sample_ptr input_samps = input->run(current_block);
            // copy one channel at a time because input may be single
            // channel that gets expanded via input_stride to input_chans,
            // which is the number of channels we always send via O2:
            for (int i = 0; i < input_chans; i++) {
                out_blob.add_samples(input_samps);
                input_samps += input_stride;
            }
            assert(out_blob.next <= out_blob.frames);
            if (out_blob.is_full()) {  // send 'em
                if (buffer.get_fifo_len() == 0) {
                    // have not received what we sent for too long
                    out_blob.next = 0;  // clear the blob, drop the samples
                    if (blocked_state == 0) {
                        blocked_state = 1;
                        send_hello();
                        restart_frame_count = frame_count;
                    } else if (blocked_state == 1 &&
                               (frame_count - restart_frame_count > AR * 3)) {
                        send_hello();
                        restart_frame_count = frame_count;
                    }
                } else {
                    if (blocked_state == 2 &&
                        (frame_count - restart_frame_count > AR * 3)) {
                        blocked_state = 0;
                    }
                    const char *data_addr = complete_address("data");
                    o2sm_send_start();
                    o2sm_add_int32(id);
                    o2sm_add_time(o2sm_time_get());
                    // compute the frame count of the first frame in blob:
                    o2sm_add_int64(frame_count + BL - out_blob.frames);
                    out_blob.add_blob();
                    o2sm_send_finish(0, data_addr, true);
                }
            }
        }
        if (has_output) {
            // printf("o2audioio output: frame_count %lld fifo_len %d "
            //        "next_buffer_frame %lld\n",
            //        frame_count, buffer.get_fifo_len(), next_buffer_frame);
            if (buffer.get_fifo_len() > 0) {
                if (floattype) {
                    assert(buffer.blocksize == chans * BL * sizeof(float));
                    buffer.dequeue((char *) out_samps);
                } else {   // assume 16-bit
                    assert(buffer.blocksize == chans * BL * sizeof(int16_t));
                    buffer.dequeue_16bit(out_samps);
                }
            } else {
                arco_print("o2audioio: underflow, id %d\n", id);
                assert(buffer.get_fifo_len() == 0);
                assert(frame_count == next_buffer_frame);
                assert(output.size() >= BL * chans);
                block_zero_n(output.get_array(), chans);
                next_buffer_frame += BL;
            }
        }
        frame_count += BL;
    }
};


