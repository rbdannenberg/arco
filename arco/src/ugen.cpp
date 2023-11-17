/* ugen.cpp -- Unit Generator
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include "arcougen.h"
#include "const.h"
#include "audioio.h"


Vec<Ugen_ptr> ugen_table;
int ugen_table_free_list = 1;
char control_service_addr[64] = "";
int control_service_addr_len = 0;

// note table really goes from 0 to 1 over index range 2 to 102
// there are 2 extra samples at either end to allow for interpolation and
// off-by-one errors
float raised_cosine[COS_TABLE_SIZE + 5] = { 
    0, 0, 0, 0.00024672, 0.000986636, 0.00221902,
    0.00394265, 0.00615583, 0.00885637, 0.0120416, 0.0157084,
    0.0198532, 0.0244717, 0.0295596, 0.0351118, 0.0411227, 0.0475865,
    0.0544967, 0.0618467, 0.069629, 0.077836, 0.0864597, 0.0954915,
    0.104922, 0.114743, 0.124944, 0.135516, 0.146447, 0.157726,
    0.169344, 0.181288, 0.193546, 0.206107, 0.218958, 0.232087,
    0.245479, 0.259123, 0.273005, 0.28711, 0.301426, 0.315938,
    0.330631, 0.345492, 0.360504, 0.375655, 0.390928, 0.406309,
    0.421783, 0.437333, 0.452946, 0.468605, 0.484295, 0.5, 0.515705,
    0.531395, 0.547054, 0.562667, 0.578217, 0.593691, 0.609072,
    0.624345, 0.639496, 0.654508, 0.669369, 0.684062, 0.698574,
    0.71289, 0.726995, 0.740877, 0.754521, 0.767913, 0.781042,
    0.793893, 0.806454, 0.818712, 0.830656, 0.842274, 0.853553,
    0.864484, 0.875056, 0.885257, 0.895078, 0.904508, 0.91354,
    0.922164, 0.930371, 0.938153, 0.945503, 0.952414, 0.958877,
    0.964888, 0.97044, 0.975528, 0.980147, 0.984292, 0.987958,
    0.991144, 0.993844, 0.996057, 0.997781, 0.999013, 0.999753, 1, 1, 1};

// safely copy ctrlservice to control_service_addr -- this is who we
//   send action messages to.  Allow 20 bytes for the rest of the address,
//   and 3 bytes for "!", "/", and EOS. Result is "!<ctrlservice>/"
int set_control_service(const char *ctrlservice)
{
    const size_t csalen = strlen(ctrlservice);
    if (csalen + 23 > sizeof(control_service_addr)) {
        arco_warn("Control service name is too long: %s.",
                  control_service_addr);
        control_service_addr[0] = 0;
        control_service_addr_len = 0;
        return 1;  // error return
    }
    control_service_addr_len = (int) (csalen + 2);
    control_service_addr[0] = '!';
    strcpy(control_service_addr + 1, ctrlservice);
    strcat(control_service_addr, "/");
    return 0;  // normal, no error
}


Initializer *initializer_list = NULL;

Initializer::Initializer(void (*f)())
{
    next = initializer_list;
    fn = f;
    initializer_list = this;
}


void Initializer::init()
{
    Initializer *d = initializer_list;
    while (d) {
        (*(d->fn))();
        d = d->next;
    }
}


Ugen::~Ugen() {
    // special case: check for run set or output set references
    assert((flags & IN_RUN_SET) == 0);
    printf("Ugen delete %d\n", id);
}


void Ugen::unref() {
    refcount--;
    printf("Ugen::unref id %d %s new refcount %d\n",
           id, classname(), refcount);
    if (refcount == 0) {
        delete this;
    }
}

// Note: since this is the only handler in this file, we register
// the handler in audioio.cpp and do not use machine-generated
// interfaces:
void arco_term(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float dur = argv[1]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, id, "arco_term");
    ugen->term(dur);
}


void Ugen::indent_spaces(int indent)
{
    for (int i = 0; i < indent; i++) {
        arco_print("  ");
    }
}


void Ugen::print(int indent, const char *param) {
    arco_print("%s_%d(%s) refs %d chans %d ",
               classname(), id, param, refcount, chans);
    print_details(indent);
    arco_print("\n");
}    


// this method runs a mark and print algorithm when print is true
// it simply unmarks everything when print is false
//
void Ugen::print_tree(int indent, bool print_flag, const char *param)
{
    if (print_flag) {
        if (flags & UGEN_MARK) {
            return;  // cut off the search; we've been here before
        }
        indent_spaces(indent);
        print(indent, param);
        flags |= UGEN_MARK;
    } else {
        if (!(flags & UGEN_MARK)) {
            return;  // cut off the search; we've been here before
        }
        flags &= ~UGEN_MARK;  // clear flag
    }
    print_sources(indent + 1, print_flag);
}


void Ugen::const_set(int chan, Sample x, const char *from)
// Assume this is a Const. Set channel chan of the const output to x.
// Do nothing but print warnings if this is not a Const ugen.
// Normally, this is called to implement a "set_input" or in general
// "set_xxxx" where xxxx is a signal input to another ugen, so from
// is a string name of the method, e.g. "Upsample::set_input".
{
    if (rate != 'c') {
        arco_warn("%s: ugen (%d) is not a Const\n", from, id);
    } else {
        ((Const *) this)->set_value(chan, x, from);
    }
}



/* /arco/free id id id ...
 */
void arco_free(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap;
    while ((ap = o2_get_next(O2_INT32))) {
        int id = ap->i;
        ANY_UGEN_FROM_ID(ugen, id, "arco_free");
        if (ugen) {
            // printf("arco_free handler freeing %d #%d\n", id, ugen->refcount);
            printf("arco_free %d (%s)\n", id, ugen->classname());
            ugen->unref();
            ugen_table[id] = NULL;  // must happen *after* destructor
        }
    }
}


void ugen_initialize()
{
    ugen_table.init(UGEN_TABLE_SIZE, true);  // fill with zero
    Initializer::init();  // add all message handlers to O2sm

    o2sm_method_new("/arco/free", NULL, arco_free, NULL, false, false);
}


// Look up the Ugen associated with id: if id is out of bounds or
// there is no ugen, or the ugen has an unexpected class, return NULL.
// If the class of the Ugen does not matter, pass NULL for classname.
Ugen_ptr id_to_ugen(int32_t id, const char *classname, const char *operation)
{
    if (!ugen_table.bounds_check(id)) {
        arco_error("Bad ugen index %d in %s\n", id, operation);
        return NULL;
    }
    Ugen_ptr ugen = ugen_table[id];
    if (!ugen) {
        arco_error("%s uninitialized id %d, ignored\n", operation, id);
    } else if (classname && (ugen->classname() != classname)) {
        arco_error("%s id %d found a %s Ugen instead of %s, ignored\n",
                   operation, id, ugen->classname(), classname);
        ugen = NULL;
    }
    return ugen;
}


// float vector x += y
void block_add_n(Sample_ptr x, Sample_ptr y, int n)
{
    n *= BL;
    for (int i = 0; i < n; i++) {
        *x++ += *y++;
    }
}
