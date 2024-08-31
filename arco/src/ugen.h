/* ugen.h -- Unit Generator
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#define INT16_TO_FLOAT(x) ((x) * 3.0518509476e-5)
// This maps to range -32767 to 32767:
#define FLOAT_TO_INT16(x) ((int) ((32767 * (x) + 32768.5)) - 32768)

// flags for Unit generators to cooperate with audioio, etc.
const int IN_RUN_SET = 1;
const int UGEN_MARK = 2;      // used for graph traversal
const int CAN_TERMINATE = 4;  // allows end in input to terminate
const int TERMINATING = 8;    // Are we terminating?  We detected inputs
// are all zero or Mix or Sum have no more inputs. Running for tail_blocks
// more sample blocks.
const int TERMINATED = 16;    // have we terminated?  After termination,
// it can be assumed that the output signal in all channels is zero.
const int UGENTRACE = 32;     // print debugging info on this ugen. Mainly
// to confirm that destructors run on ugens.

// special Unit generator IDs:
const int ZERO_ID = 0;
const int ZEROB_ID = 1;

extern char control_service_addr[64];  // where to send messages
        // this is set in audioio.cpp by /arco/open messages and is
        // "!" followed by the service followed by "/" so that you can
        // append the rest of the address in place.
extern int control_service_addr_len;  // address len including "!" and "/"

// used by recplay and fader Ugens:
#define COS_TABLE_SIZE 100
extern float raised_cosine[COS_TABLE_SIZE + 5];

// used by math and mathb:
const int MATH_OP_MUL = 0;
const int MATH_OP_ADD = 1;
const int MATH_OP_SUB = 2;
const int MATH_OP_DIV = 3;
const int MATH_OP_MAX = 4;
const int MATH_OP_MIN = 5;
const int MATH_OP_CLP = 6;  // min(max(x, -y), y) i.e. clip if |x| > y
const int MATH_OP_POW = 7;
const int MATH_OP_LT = 8;
const int MATH_OP_GT = 9;
const int MATH_OP_SCP = 10;
const int MATH_OP_PWI = 11;
const int MATH_OP_RND = 12;
const int MATH_OP_SH = 13;
const int MATH_OP_QNT = 14;
#define NUM_MATH_OPS 15

extern const char *OP_TO_STRING[NUM_MATH_OPS];

// IMPORTANT: x1 and x2 should be variables, not expressions
// since they are evaluated multiple times. x1 is modified
//
#define SOFTCLIP(x1, x2)  \
    ((x2 <= 0) ? 0 : \
     (x1 = copysignf(fminf(fabsf(x1), x2), x1), \
      x1 /= x2, \
      (x1 * 3 - x1 * x1 * x1) * 0.5 * x2))


int set_control_service(const char *ctrlservice);

class Ugen;
typedef Ugen *Ugen_ptr;
extern Vec<Ugen_ptr> ugen_table;

extern void ugen_initialize();

class Initializer {
  public:
    Initializer *next;
    void (*fn)();

    Initializer(void (*f)());  // constructor adds function to a list
    static void init();  // init calls all functions on the list
};


class Ugen : public O2obj {
  public:
    int id;
    int refcount;
    // tail_blocks tells how many blocks to compute beyond input
    // termination; initialized to zero, but the subclass constructor
    // or other method can set it to a higher value.  When an input
    // terminates, this variable counts down to zero, then this Ugen
    // terminates:
    int tail_blocks;
    char flags;
    char rate;
    int chans;
    Vec<Sample> output;
    Sample_ptr out_samps;  // pointer to actual sample memory
    int current_block;
    int action_id;

    // to initialize a Ugen without installing in table, pass id = -1
    Ugen(int id, char ab, int nchans) {
        refcount = 1;
        if (id >= 0) {
            if (ugen_table.bounds_check(id)) {
                if (ugen_table[id]) {
                    arco_warn("Ugen ID %d is already taken. Freeing the ID",
                              id);
                    ugen_table[id]->unref();
                }
                ugen_table[id] = this;  // install Ugen in the table
                refcount = 1;
            } else {
                arco_error("New Ugen ID %d is out of bounds.", id);
                id = -1;  // ID for all Ugens not in the table
                // probably, this Ugen will be orphaned. We will correctly
                // set the refcount to 0, but unless the caller
                // does ->refcount++ and then ->unref(), this Ugen
                // will never be freed. Pass id=-1 to correctly create
                // a Ugen that is not in the table. The returned refcount
                // will be 1 assuming the Ugen will be referenced by some
                // other Ugen which will unref() this when it is freed.
                refcount = 0;
            }
        }
        this->id = id;
        tail_blocks = 0;
        flags = 0;
        rate = ab;
        chans = nchans;
        if (rate) {  // if no output, pass 0 for rate
            int output_size = nchans * (rate == 'a' ? BL : 1);
            output.set_size(output_size, false);
            out_samps = &output[0];
        } else {
            out_samps = NULL;
        }
        current_block = 0;
        action_id = 0;
        printf("created ugen@%p\n", this);
    }

    // subclasses should override to unref inputs
    virtual ~Ugen();

    virtual const char *classname() = 0;
    
    void indent_spaces(int indent);

    virtual void print(int indent = 0, const char *param = "",
                       bool revisiting = false);

    virtual void print_tree(int indent, bool print_flag, const char *param);
    
    virtual void print_details(int indent) { ; }  // default: none to print

    const char *btos(bool b) { return (b ? "true" : "false"); }

    // inherit print_sources if there are no inputs:
    virtual void print_sources(int indent, bool print_flag) { ; }

    void init_param(Ugen_ptr newp, Ugen_ptr &p, int &pstride) {
        // either map single channel output to all inputs, or map
        // corresponding output channels to input channels
        p = newp;
        int n = p->chans;
        if (n != 1 && n < chans) {
             arco_warn("%s: channel mismatch, using only first channel of %s\n",
                       classname(), p->classname());
             n = 1;
        }
        // 0 for mono, BL for multichannel audio, 1 for multichannel block rate
        pstride = (n == 1 ? 0 : (p->rate == 'a' ? BL : 1));
        // if an input to an a-rate Ugen is not a-rate, it is probably
        // interpolated to a-rate, which implies a 1-block "tail" while the
        // input is linearly interpolated to zero; if so, tail_blocks should be
        // at least 1. If not, computing an extra block of zeros after
        // terminating should be fairly harmless. We never try to restore
        // tail_blocks to zero since we don't know in this call if there are
        // other b-rate inputs and it seems silly to do more work.
        if (rate == 'a' && (p->rate != 'a') && tail_blocks == 0) {
            tail_blocks = 1;
        }
        p->ref();
    }
        
    // get the ith block of output samples (for 'a' rate only)
    Sample_ptr outblock(int ch) { return &output[ch * BL]; }
    
    void ref() { refcount++; }
    virtual void unref();
    
    virtual void real_run() = 0;

    virtual Sample_ptr run(int block_count) {
        Sample_ptr save_out_samps = out_samps;
        if (block_count > current_block) {
            current_block = block_count;
            real_run();
        }
        out_samps = save_out_samps;
        return save_out_samps;
    }


    virtual void on_terminate() { send_action_id(); }  // override if needed


    // declare that when an input terminates, this ugen should continue
    // computing for tail_sec (seconds) and then terminate (default is 0)
    void term(float tail_sec) {
        if (!(flags & (TERMINATING | TERMINATED))) {
            tail_blocks = std::ceil(tail_sec * BR);
            flags |= CAN_TERMINATE;
        } // else we're already terminating; do not reset the counter.
    }


    virtual void terminate() { // this Ugen will terminate after tail blocks
        if (!(flags & TERMINATING)) {
            flags |= TERMINATING;  // start the count
        }
        if (tail_blocks-- == 0) {
            flags |= TERMINATED;
            on_terminate();  // a virtual method called at most once
        }
    }
 

    void set_current_block(int n) { current_block = n; }

    void const_set(int chan, Sample x, const char *from);

    void send_action_id(int status = 0) {
        if (action_id == 0) return;
        o2sm_send_start();
        o2sm_add_int32(action_id);
        o2sm_add_int32(status);
        strcpy(control_service_addr + control_service_addr_len, "act");
        printf("send_action_id address %s status(actl) %d\n",
               control_service_addr, o2_status("actl"));
        o2sm_send_finish(0.0, control_service_addr, true);
        action_id = 0;  // one-shot
    }
};


typedef Ugen *Ugen_ptr;

Ugen_ptr id_to_ugen(int32_t id, const char *classname, const char *operation);

// Use this to convert an id to a Ugen of a given class.
// This macro expands to 2 statements, and defines a variable, and it
// issues a return from a void function on error. Example:
//     UGEN_FROM_ID(Pwl, pwl,id, "arco_pwl_start");
// defines the local variable Pwl *pwl or returns on error.
// 
#define UGEN_FROM_ID(cn, var, id, op)                                   \
        cn *var = (cn *) id_to_ugen(id, cn##_name, op); if (!var) return;

// Use this to convert an id to any Ugen
#define ANY_UGEN_FROM_ID(var, id, op) \
    Ugen *var = id_to_ugen(id, NULL, op); if (!var) return;

#define block_copy(dst, src) memcpy(dst, src, BLOCK_BYTES)

// copy n channels of blocks:
#define block_copy_n(dst, src, n) memcpy(dst, src, BLOCK_BYTES * (n))

#define block_zero(dst) memset(dst, 0, BLOCK_BYTES)

// zero n channels of blocks:
#define block_zero_n(dst, n) memset(dst, 0, BLOCK_BYTES * (n))

void block_add_n(Sample_ptr x, Sample_ptr y, int n);

void arco_term(O2SM_HANDLER_ARGS);  // normally these handlers are local
// to the compilation unit (.cpp file), but this one is defined in
// ugen.cpp but registered with o2sm_method_new() in audioio.cpp.
