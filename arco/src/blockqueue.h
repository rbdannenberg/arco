/* blockqueue.h -- ring (circular) buffer for blocks of Samples
 *
 * Roger B. Dannenberg
 * Aug 2024
 */

/* Simple FIFO of Samples. See also ringbuf.h. This class is optimized
   for block-at-a-time insert and remove and allows queues of any
   fixed size.  When head == tail, the FIFO is empty, and * since tail
   always points to an available index, the maximum length * of the
   FIFO is one block less than the allocated space. By working in units
   of blocksize, we can always memcpy a fulll block directly to a single
   address rather than worry about blocks that "wrap" around the circular
   queue structure.
 */

class Blockqueue : public Vec<char> {
  public:
    int head;   // index in blocksize units of where to remove the next item
    int tail;   // index in blocksize units of where to insert the next item
    int blocksize;  // how long is each "block" in the queue
    int queuelen;   // how long is the logical queue in blocks?

    // size is the number of blocks that the queue can hold
    Blockqueue(int blocksize, int size = 0, bool z = false) :
            Vec<char>(0) { init(blocksize, size, z); }


    // Create a circular buffer with capacity for size samples, a multiple of BL.
    // If z is false (default), the initial length is zero. If z is true,
    // the buffer is initialized with size zeros.
    void init(int blocksize_, int size, bool z = false) {
        blocksize = blocksize_;
        head = 0;
        tail = (z ? size : 0);
        Vec<char>::init(0);
        Vec<char>::set_size((size + 1) * blocksize, z);
    }

    
    int get_fifo_len() {
        if (tail >= head) {
            return tail - head;
        } else {
            return size() / blocksize + tail - head;
        }
    }

    
    void enqueue(char *s_ptr) {
    // enqueue blocksize bytes from s_ptr.
        // we add one blocksize and storage area (size())
        // can hold a max of size() - blocksize:
        assert(get_fifo_len() + blocksize <= size() - blocksize);

        memcpy(&((*this)[tail * blocksize]), s_ptr, blocksize);
        tail++;
        if (tail * blocksize == size()) {
            tail = 0;
        }
    }


    void enqueue_zeros() {
    // enqueue blocksize zero bytes
        // we add one blocksize and storage area (size())
        // can hold a max of size() - blocksize:
        assert(get_fifo_len() + blocksize <= size() - blocksize);
        memset(&((*this)[tail * blocksize]), 0, blocksize);
        tail++;
        if (tail * blocksize == size()) {
            tail = 0;
        }
    }        


    /*
    void enqueue_16bit(float *s_ptr) {
    // special case: enqueue blocksize bytes floats at s_ptr,
    // converting them to 16-bit samples in the queue
        assert(get_fifo_len() + blocksize <= size() - blocksize);
        int16_t *dest = (int16_t *) &((*this)[tail * blocksize]);
        for (i = 0; i < blocksize / 2; i++) {
            *dest++ = INT16_TO_FLOAT(*s_ptr++);
        }
        tail++;
        if (tail * blocksize == size()) {
            tail = 0;
        }
    }
    */  


    void dequeue(char *s_ptr) {
    // dequeue n samples and copy them to s_ptr
        assert(get_fifo_len() > 0);
        memcpy(s_ptr, &((*this)[head]), blocksize);
        head++;
        if (head * blocksize == size()) {
            head = 0;
        }
    }


    void dequeue_16bit(float *s_ptr) {
    // special case: dequeue blocksize bytes of 16-bit samples,
    // converting them to floats and storing the result at s_ptr
        assert(get_fifo_len() > 0);
        int16_t *src = (int16_t *) &((*this)[head * blocksize]);
        for (int i = 0; i < blocksize / 2; i++) {
            *s_ptr++ = INT16_TO_FLOAT(*src++);
        }
        head++;
        if (head * blocksize == size()) {
            head = 0;
        }
    }


    void toss() {
    // remove blocksize bytes from head (data is unused)
        head++;
        if (head * blocksize == size()) {
            head = 0;
        }
    }
};
