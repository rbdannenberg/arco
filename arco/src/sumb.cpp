/* sumb.cpp -- unit generator that adds inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

#include "arcougen.h"
#include "sumb.h"

const char *Sumb_name = "Sumb";

/* O2SM INTERFACE: /arco/sumb/new int32 id, int32 chans;
 */
void arco_sumb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(sumb_ugen, id, "arco_sumb_new");

    new Sumb(id, chans);
}


/* O2SM INTERFACE: /arco/sumb/ins int32 id, int32 x_id;
 */
static void arco_sumb_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Sumb, sumb, id, "arco_sumb_ins");
    ANY_UGEN_FROM_ID(x, x_id, "arco_sumb_ins");
    sumb->ins(x);
}


/* O2SM INTERFACE: /arco/sumb/rem int32 id, int32 x_id;
 */
static void arco_sumb_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Sumb, sumb, id, "arco_sumb_rem");
    ANY_UGEN_FROM_ID(x, x_id, "arco_sumb_rem");
    sumb->rem(x);
}


/* O2SM INTERFACE:  /arco/sumb/swap int32 id, int32 id1, int32 id2;
 *    replace first ugen with the second one. This is an atomic
 *    operation to support inserting a fadeout between a source and
 *    the output when the source is already connected and playing,
 *    avoiding any race condition.
 */
static void arco_sumb_swap(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t id1 = argv[1]->i;
    int32_t id2 = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Sumb, sumb, id, "arco_sumb_swap");
    ANY_UGEN_FROM_ID(ugen, id1, "arco_sumb_swap");
    ANY_UGEN_FROM_ID(replacement, id2, "arco_sumb_swap");
    if (replacement->rate != 'b') {
        arco_warn("/arco/sumb/swap id2 (%d) is not block rate\n", id2);
        return;
    }
    sumb->swap(ugen, replacement);
    printf("arco_sumb_swap: removed %d, inserted %d in output set\n", id, id2);
}



static void sumb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/sumb/new", "ii", arco_sumb_new, NULL, true, true);
    o2sm_method_new("/arco/sumb/ins", "ii", arco_sumb_ins, NULL, true, true);
    o2sm_method_new("/arco/sumb/rem", "ii", arco_sumb_rem, NULL, true, true);
    o2sm_method_new("/arco/sumb/swap", "iii", arco_sumb_swap, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer sumb_init_obj(sumb_init);
