/* ugen.cpp -- Unit Generator
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#include "arcougen.h"
#include "const.h"


Vec<Ugen_ptr> ugen_table;

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


const char *OP_TO_STRING[NUM_MATH_OPS] = {
    "MATH_OP_MUL", "MATH_OP_ADD", "MATH_OP_SUB", "MATH_OP_DIV", "MATH_OP_MAX",
    "MATH_OP_MIN", "MATH_OP_CLP", "MATH_OP_POW", "MATH_OP_LT", "MATH_OP_GT",
    "MATH_OP_SCP", "MATH_OP_POWI", "MATH_OP_RND", "MATH_OP_SH", "MATH_OP_QNT",
    "MATH_OP_RLI", "MATH_OP_HZDIFF", "MATH_OP_TAN", "MATH_OP_ATAN" };


const char *UNARY_OP_TO_STRING[NUM_UNARY_OPS] = {
    "UNARY_OP_ABS", "UNARY_OP_NEG", "UNARY_OP_EXP",
    "UNARY_OP_LOG", "UNARY_OP_LOG10", "UNARY_OP_LOG2", "UNARY_OP_SQRT",
    "UNARY_OP_STEP_TO_HZ", "UNARY_OP_HZ_TO_STEP", "UNARY_OP_VEL_TO_LINEAR",
    "UNARY_OP_LINEAR_TO_VEL", "UNARY_OP_DB_TO_LINEAR",
    "UNARY_OP_LINEAR_TO_DB" };


Ugen::~Ugen() {
    // send_action_id(ACTION_FREE);
    // special case: check for run set or output set references
    assert((flags & IN_RUN_SET) == 0);
    if (id >= 0) {  // we are still in the ugen table, which now has an
                    // invalid pointer: remove the reference from the table
        ugen_table[id] = NULL;
    }
    // printf("Ugen delete %d\n", id);
}

#if ARCO_REF_DEBUG
bool Ugen::find_reference_to(Ugen *ugen) {
    if (this == ugen) {
        return true;  // found reference to ugen after ugen's refcount was 0
    }
    if (ref_debug_mark) {  // we've been here before and didn't find ugen
        return false;
    }
    int i = 0;
    Ugen *child;
    ref_debug_mark = true;
    while (get_ref(i, &child)) {
        if (child && child->find_reference_to(ugen)) {
            printf("Ugen %d (%s) refcount is 0 but %d (%s) has a reference"
                   " to it\n", ugen->id, ugen->classname(), id, classname());
            assert(false);
        }
        i++;
    }
    ref_debug_mark = false;  // clear it so we can search again
    return false;
}
#endif

// unref - decrement reference count and possibly free Ugen. The ptr parameter
//     should be the address of the actual reference that is being removed, and
//     NULL will be written to the address to actually remove it. This is
//     redundant if the referencer itself is in the process of being freed, but
//     this is a "safe programming feature" to help make sure the reference is
//     not used again. It is easy to circumvent, e.g. by passing the address of
//     a copy of the reference, but at least it serves as a strong reminder that
//     the pointer should become NULL to aid in debugging and avoid accidental
//     access through an invalid pointer because some other code did not properly
//     maintain the reference count.
void Ugen::unref(Ugen **ptr) {
#if ARCO_REF_DEBUG
    *ptr = NULL;  // remove the pointer pointing to this Ugen so that the graph will
                  // match the reference counts after we decrement refcount.
#endif
#if ARCO_REF_DEBUG > 1
    arco_print("Ugen::unref id %d class %s old_refcount %d ptr %p\n",
               id, classname(), refcount, this);
#endif
    refcount--;
    // printf("Ugen::unref id %d %s new refcount %d\n",
    //        id, classname(), refcount);
    if (refcount == 0) {
#if ARCO_REF_DEBUG
        // make sure there are no more references to this ugen
        for (int i = 0; i < UGEN_TABLE_SIZE; i++) {
            if (i != id && ugen_table[i] &&
                ugen_table[i]->find_reference_to(this)) {
                printf("Ugen %d (%s) refcount is 0 but table[%d] (%s) has a"
                       " (indirect?) reference to it\n",
                       id, classname(), i, ugen_table[i]->classname());
                assert(false);
            }
        }
#endif
        on_terminate(ACTION_FREE);  // notify `atend` mechanism if it is
        // pending this should be in destructor, but destructors cannot
        // call inherited methods in c++.
        if (flags & UGENTRACE) {
            arco_print("Deleting traced ugen: "); print();
        }
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


// Both param ("") and revisiting (false) are optional
void Ugen::print(int indent, const char *param, bool revisiting) {
    const char *openp = "(";
    const char *closep = ") ";
    if (!*param) {
        openp = closep = "";
    }
    const char *was = "";
    int posid = id;
    if (id < 0) {
        was = "(was) ";
        posid = -id;
    }
    arco_print("%s %s%s%sid %s%d refs %d chans %d ",
               classname(), openp, param, closep, was, posid, refcount, chans);
    if (flags & TERMINATED) {
        arco_print("TERMINATED ");
    } else if (flags & TERMINATING) {
        arco_print("terminate in %g sec ", tail_blocks * BP);
    } else if (flags & CAN_TERMINATE) {
        arco_print("can terminate ");
    }
    print_details(indent);
    if (revisiting) {
        arco_print(" (shown above)");
    }
    arco_print("\n");
}    


// this method runs a mark and print algorithm when print is true
// it simply unmarks everything when print is false
//
void Ugen::print_tree(int indent, bool print_flag, const char *param)
{
    bool visited = ((flags & UGEN_MARK) != 0);
    if (print_flag) {
        indent_spaces(indent);
        print(indent, param, visited);
        flags |= UGEN_MARK;
        if (visited) {
            return;
        }
    } else {
        if (!visited) {
            return;  // cut off the search; we've been here before
        }
        flags &= ~UGEN_MARK;  // clear flag
    }
    // tricky logic: if we're printing and haven't visited this ugen
    // yet then we print the sub-tree; otherwise, if we're not printing
    // and just clearing all the flags, we get here if the flag is set
    // (visited == true), so we visit the subtree to clear subtree flags.
    // In both cases, if we are here, then print_flag == !visited.
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


/* /arco/act id action_id action_mask */
void arco_act(O2SM_HANDLER_ARGS)
{
    int32_t id = argv[0]->i;
    ANY_UGEN_FROM_ID(ugen, id, "arco_act");
    ugen->action_id = argv[1]->i;
    // ACTION_FREE is always sent when ugen is freed if there is an action_id
    ugen->action_mask = (argv[2]->i) | ACTION_FREE;
}


void arco_trace(O2SM_HANDLER_ARGS)
{
    int32_t id = argv[0]->i;
    int32_t trace = argv[1]->i;

    ANY_UGEN_FROM_ID(ugen, id, "arco_trace");
    if (trace) {
        arco_print("Tracing ugen: "); ugen->print();
        ugen->flags |= UGENTRACE;
    } else {
        ugen->flags &= ~UGENTRACE;
    }
}


/* /arco/free id id id ...  */
void arco_free(O2SM_HANDLER_ARGS)
{
    o2_extract_start(msg);
    O2arg_ptr ap;
    while ((ap = o2_get_next(O2_INT32))) {
        int id = ap->i;
        ANY_UGEN_FROM_ID(ugen, id, "arco_free");
        if (ugen) {
            // printf("*** arco_free: %d #%d (%s)\n", id, ugen->refcount,
            //       ugen->classname());
            ugen->unref(&ugen);
            if (ugen_table[id]) {  // id is no longer valid; Ugen still exists
                // printf("*** arco_free: %d #%d (%s) becomes %d\n", id,
                //       ugen->refcount, ugen->classname(), -id);
                ugen->id = -id;  // remember Ugen's original id to aid
                ugen_table[id] = NULL;
                // in debugging (but be aware that when negative, the id may not
                // be unique. Also, 0 (the zero Ugen) is a special case, but
                // ugen_table[0] should never be freed or replaced unless Arco is
                // reset.
            }  // else the Ugen is freed, and it set ugen_table[id] to NULL
        }
    }
}


void ugen_initialize()
{
    ugen_table.init(UGEN_TABLE_SIZE, true);  // fill with zero, exact size
    Initializer::init();  // add all message handlers to O2sm

    o2sm_method_new("/arco/free", NULL, arco_free, NULL, false, false);
    o2sm_method_new("/arco/act", "iii", arco_act, NULL, true, true);
    o2sm_method_new("/arco/trace", "ii", arco_trace, NULL, true, true);
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
