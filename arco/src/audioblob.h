/* audioblob.h -- implements an O2 blob that contains either
 *        16-bit or float audio data
 * Roger B. Dannenberg
 * Aug, 2024
 */

void blob_byteswap(char *data, int frames, int chans, bool floattype);

class Audioblob {
  public:
    bool floattype;  // false for int16, true for float
    int chans;       // how many channels
    int frames;      // how many frames - multiple of BL
    int next;        // count of how many frames written so far
    O2blob_ptr blob;  // allocated with o2_blob_new()

    Audioblob(bool floattype_, int chans_, int frames_) {
        assert((frames_ / BL) * BL == frames_);  // multiple of BL
        floattype = floattype_;
        chans = chans_;
        frames = frames_;
        next = 0;
        if (chans > 0) {
            blob = o2_blob_new(frames * chans * 2 * (floattype + 1));
        } else {
            blob = NULL;
        }
    }

    ~Audioblob() {
        if (blob) {
            O2_FREE(blob);
        }
    }

    bool is_full() { return next == frames; }

    void add_samples(Sample_ptr src) {
    // add one block of samples to content of blob, converting to 16-bit
    // if not floattype:
        if (floattype) {
            float *outptr = ((float *) blob->data) + next * chans;
            block_copy(outptr, src);
        } else {  // 16-bit
            int16_t *outptr = ((int16_t *) blob->data) + next * chans;
            for (int i = 0; i < BL; i++) {
                 *outptr++ = FLOAT_TO_INT16(*src++);
            }
        }
        next += BL;
        assert(next <= frames);
    }

    void add_blob() {
        // add blob using o2sm_add_blob after byte-swapping to network order
        blob_byteswap(blob->data, frames, chans, floattype);
        o2sm_add_blob(blob);
        next = 0;  // start refilling message after sending
    }
};
