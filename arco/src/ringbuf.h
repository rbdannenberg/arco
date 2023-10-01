/* ringbuf.h -- ring (circular) buffer for Samples
 *
 * Roger B. Dannenberg
 * Sep 2023
 */

/* Simple FIFO of Samples. When head == tail, the FIFO is empty, and
 * since tail always points to an available index, the maximum length
 * of the FIFO is one less than the allocated space, which in this
 * implementation is a power of 2. Therefore, a size of 2^N - 1 will
 * make optimal use of all available space, and a size of 2^N will
 * use at most half of the allocated space. Given current memory
 * allocation methods and the underlying Vec implementation, there
 * will generally be additional unused memory because large memory
 * chunks come in powers of two sizes.
 */

class Ringbuf : public Vec<Sample> {
  public:
    int head;   // index of where to remove the next item
    int tail;   // index of where to insert the next item
    int mask;   // aid for wrapping around memory buffer
    

    Ringbuf(int size = 0, bool z = false) :
            Vec<Sample>(size, z) { init(size, z); }


    int vec_len(int size) {
    // get power of 2 >= size + 1, works for up to size = 2^32 - 1
        if (size > 0) {
            size |= size >> 1;  // trick is to fill from the left-most 1
            size |= size >> 2;  // until 1's fill all the lower bits
            size |= size >> 4;  // adding 1 to 111...1 gives power of 2
            size |= size >> 8;  // in the form 1000...0
            size |= size >> 16;
            size++;
        }
        return size;
    }

    
    // Create a circular buffer with capacity for at least size samples.
    // If z is false (default), the initial length is zero. If z is true,
    // the buffer is initialized with size zeros.
    void init(int size, bool z = false) {
        head = 0;
        tail = (z ? size : 0);
        Vec<Sample>::init(0);
        Vec<Sample>::set_size(vec_len(size));
        mask = length > 0 ? length - 1 : 0;
    }

    
    void enqueue(Sample s) {
        array[tail++] = s;
        tail &= mask;
    }

    
    void enqueue_block(Sample *block) {
        for (int i = 0; i < BL; i++) {
            enqueue(block[i]);
        }
    }

    
    Sample dequeue() {
        Sample s = array[head++];
        head &= mask;
        return s;
    }


    void toss(int n) {
    // remove n samples from head (samples are unused)
        head = (head + n) & mask;
    }
    
    
    void dequeue_block(Sample *block) {
        for (int i = 0; i < BL; i++) {
            block[i] = dequeue();
        }
    }


    int get_fifo_len() {
        return (tail - head) & mask;
    }


    Sample get_nth(int n) {
    // get element at tail - n (with wrap), e.g. in an audio delay, read
    // samples using get_nth(round(audiorate * delay))
        return (*this)[(tail - n) & mask];
    }


    void add_to_nth(Sample x, int n) {
    // add x to the queue element at tail - n (with wrap). This is very
    // specialized and used in granstream where we want to add delayed
    // output to one grainstream's delay buffer.
        (*this)[(tail - n) & mask] += x;
    }

    
    void set_fifo_len(int len, bool longer = false) {
    // change the length of FIFO. If longer, we want the queue
    // to get longer by inserting zeros at the head until the
    // length is len. If *not* longer, we want to expand the buffer
    // as needed to support len samples, but number of samples in
    // the queue does not change.
        int old_len = length;
        int fifo_len = get_fifo_len();
        int n = length - old_len;  // how much did we grow?
        if (len >= length) {  // we need at least len + 1
            set_size(vec_len(len));
            mask = size() - 1;
        }
        if (len > fifo_len) {
            // now queue looks like [tuvwxT...Hpqrs?????], where Hpqrs is
            // the head of the queue and tuvwxT is the tail. We want to
            // convert to [tuvwxT...xxxxxHpqrs] if longer is false, and
            //            [tuvwxT...xxxH00pqrs] if longer is true, where
            // xxx is the old Hpqrs data which has been moved
            // First, if head > tail, move Hpqrs to the end of new vec.array:
            if (head > tail) {
                Sample *headptr = array + head;
                int to_end = fifo_len - head;
                memmove(headptr, headptr + n, to_end * sizeof(Sample));
                // check: we want headptr + n + to_end == array + length
                // => array + head + n + to_end == array + length
                // => head + length - fifo_len + fifo_len - head = length
                // => length = length, so we moved data to the end of array
                head += n;
            }
            // Then, if longer, insert zeros, decreasing head:
            if (longer) {
                int new_head = head - (len - fifo_len);
                // head might wrap around. If so, fill zeros to end and wrap:
                if (new_head < 0) {
                    memset(array, 0, head * sizeof(Sample));
                    // now we filled 0 to old head. Wrap and fall through:
                    new_head += length;
                    head = length;
                }
                Sample *headptr = array + new_head;
                memset(headptr, 0, (head - new_head) * sizeof(Sample));
                head = new_head;
            }
        } else if (len < fifo_len) {  // shorten queue -- does not reallocate
            // now queue looks like [tuvwxT...Hpqrs], where Hpqrs is
            // the head of the queue and tuvwxT is the tail. We want to
            // convert to [tuvwxT...xxxHs], where xxx is the old Hpqrs
            // data which has been moved.
            head += fifo_len - len;
            head &= mask;
        }
    }    
};
