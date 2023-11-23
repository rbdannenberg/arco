/* sum.cpp -- unit generator that sums inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

#include "arcougen.h"
#include "sum.h"

const char *Sum_name = "Sum";

/* O2SM INTERFACE: /arco/sum/new int32 id, int32 chans, int32 wrap;
 */
void arco_sum_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t wrap = argv[2]->i;
    // end unpack message

    new Sum(id, chans, wrap);
}


/* O2SM INTERFACE: /arco/sum/gain int32 id, float gain;
 */
static void arco_sum_gain(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float gain = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Sum, sum, id, "arco_sum_ins");
    sum->gain = gain;
}


/* O2SM INTERFACE: /arco/sum/ins int32 id, int32 x_id;
 */
static void arco_sum_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Sum, sum, id, "arco_sum_ins");
    ANY_UGEN_FROM_ID(x, x_id, "arco_sum_ins");
    sum->ins(x);
}


/* O2SM INTERFACE: /arco/sum/rem int32 id, int32 x_id;
 */
static void arco_sum_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Sum, sum, id, "arco_sum_rem");
    ANY_UGEN_FROM_ID(x, x_id, "arco_sum_rem");
    sum->rem(x);
}


/* O2SM INTERFACE:  /arco/sum/swap int32 id, int32 id1, int32 id2;
 *    replace first ugen with the second one. This is an atomic
 *    operation to support inserting a fadeout between a source and
 *    the output when the source is already connected and playing,
 *    avoiding any race condition.
 */
static void arco_sum_swap(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t id1 = argv[1]->i;
    int32_t id2 = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Sum, sum, id, "arco_sum_swap");
    ANY_UGEN_FROM_ID(ugen, id1, "arco_sum_swap");
    ANY_UGEN_FROM_ID(replacement, id2, "arco_sum_swap");
    if (replacement->rate != 'a') {
        arco_warn("/arco/sum/swap id2 (%d) is not audio rate\n", id2);
        return;
    }
    sum->swap(ugen, replacement);
    printf("arco_sum_swap: removed %d, inserted %d in sum %d\n", id1, id2, id);
}


static void sum_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/sum/new", "iii", arco_sum_new, NULL, true, true);
    o2sm_method_new("/arco/sum/gain", "if", arco_sum_gain, NULL, true,
                    true);
    o2sm_method_new("/arco/sum/ins", "ii", arco_sum_ins, NULL, true, true);
    o2sm_method_new("/arco/sum/rem", "ii", arco_sum_rem, NULL, true, true);
    o2sm_method_new("/arco/sum/swap", "iii", arco_sum_swap, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer sum_init_obj(sum_init);
