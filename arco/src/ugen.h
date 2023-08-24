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
const int IN_OUTPUT_SET = 1;
const int IN_RUN_SET = 2;
const int UGEN_MARK = 4;  // used for graph traversal

extern char control_service_addr[64];  // where to send messages
        // this is set in audioio.cpp by /arco/open messages and is
        // "!" followed by the service followed by "/" so that you can
        // append the rest of the address in place.
extern int control_service_addr_len;  // address len including "!" and "/"

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
    char flags;
    char rate;
    int chans;
    Vec<Sample> output;
    Sample_ptr out_samps;  // pointer to actual sample memory
    int64_t current_block;

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
        flags = 0;
        rate = ab;
        chans = nchans;
        if (rate) {  // if no output, pass 0 for rate
            output.init(nchans * (rate == 'a' ? BL : 1));
            output.set_size(output.get_allocated(), false);
        }
        current_block = 0;
    }

    // subclasses should override to unref inputs
    virtual ~Ugen() { /* printf("Ugen delete %d\n", id) */ ; }

    virtual const char *classname() = 0;
    
    void indent_spaces(int indent);

    virtual void print_tree(int indent, bool print, const char *param);

    // inherit print_sources if there are no inputs:
    virtual void print_sources(int indent, bool print) { ; }

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
        p->refcount++;
    }
        
    // get the ith block of output samples (for 'a' rate only)
    Sample_ptr outblock(int ch) { return &output[ch * BL]; }
    
    void ref() { refcount++; }
    virtual void unref();
    
    virtual void real_run() = 0;

    virtual Sample_ptr run(int64_t block_count) {
        if (block_count > current_block) {
            out_samps = &output[0];
            current_block = block_count;
            real_run();
        }
        return &output[0];
    }

    void set_current_block(int64_t n) { current_block = n; }

    void send_action_id(int &action_id, int status = 0) {
        o2sm_send_start();
        o2sm_add_int32(action_id);
        o2sm_add_int32(status);
        strcpy(control_service_addr + control_service_addr_len, "act");
        printf("send_action_id address %s status(actl) %d\n", control_service_addr,
               o2_status("actl"));
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

#define imin(x, y) ((x) <= (y) ? (x) : (y))

