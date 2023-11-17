/* add.cpp -- unit generator that adds inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

#include "arcougen.h"
#include "add.h"

const char *Add_name = "Add";

/* O2SM INTERFACE: /arco/add/new int32 id, int32 chans, int32 wrap;
 */
void arco_add_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t wrap = argv[2]->i;
    // end unpack message

    new Add(id, chans, wrap);
}


/* O2SM INTERFACE: /arco/add/gain int32 id, float gain;
 */
static void arco_add_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float gain = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Add, add, id, "arco_add_ins");
    add->gain = gain;
}


/* O2SM INTERFACE: /arco/add/ins int32 id, int32 x_id;
 */
static void arco_add_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Add, add, id, "arco_add_ins");
    ANY_UGEN_FROM_ID(x, x_id, "arco_add_ins");
    add->ins(x);
}


/* O2SM INTERFACE: /arco/add/rem int32 id, int32 x_id;
 */
static void arco_add_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Add, add, id, "arco_add_rem");
    ANY_UGEN_FROM_ID(x, x_id, "arco_add_rem");
    add->rem(x);
}


/* O2SM INTERFACE:  /arco/add/swap int32 id, int32 id1, int32 id2;
 *    replace first ugen with the second one. This is an atomic
 *    operation to support inserting a fadeout between a source and
 *    the output when the source is already connected and playing,
 *    avoiding any race condition.
 */
static void arco_add_swap(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t id1 = argv[1]->i;
    int32_t id2 = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Add, add, id, "arco_add_swap");
    ANY_UGEN_FROM_ID(ugen, id1, "arco_add_swap");
    ANY_UGEN_FROM_ID(replacement, id2, "arco_add_swap");
    if (replacement->rate != 'a') {
        arco_warn("/arco/add/swap id2 (%d) is not audio rate\n", id2);
        return;
    }
    add->swap(ugen, replacement);
    printf("arco_add_swap: removed %d, inserted %d in add %d\n", id1, id2, id);
}


static void add_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/add/new", "iii", arco_add_new, NULL, true, true);
    o2sm_method_new("/arco/add/gain", "if", arco_add_gain, NULL, true,
                    true);
    o2sm_method_new("/arco/add/ins", "ii", arco_add_ins, NULL, true, true);
    o2sm_method_new("/arco/add/rem", "ii", arco_add_rem, NULL, true, true);
    o2sm_method_new("/arco/add/swap", "iii", arco_add_swap, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer add_init_obj(add_init);
