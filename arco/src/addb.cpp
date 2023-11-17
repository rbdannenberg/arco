/* addb.cpp -- unit generator that adds inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

#include "arcougen.h"
#include "addb.h"

const char *Addb_name = "Addb";

/* O2SM INTERFACE: /arco/addb/new int32 id, int32 chans;
 */
void arco_addb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(addb_ugen, id, "arco_addb_new");

    new Addb(id, chans);
}


/* O2SM INTERFACE: /arco/addb/ins int32 id, int32 x_id;
 */
static void arco_addb_ins(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Addb, addb, id, "arco_addb_ins");
    ANY_UGEN_FROM_ID(x, x_id, "arco_addb_ins");
    addb->ins(x);
}


/* O2SM INTERFACE: /arco/addb/rem int32 id, int32 x_id;
 */
static void arco_addb_rem(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Addb, addb, id, "arco_addb_rem");
    ANY_UGEN_FROM_ID(x, x_id, "arco_addb_rem");
    addb->rem(x);
}


/* O2SM INTERFACE:  /arco/addb/swap int32 id, int32 id1, int32 id2;
 *    replace first ugen with the second one. This is an atomic
 *    operation to support inserting a fadeout between a source and
 *    the output when the source is already connected and playing,
 *    avoiding any race condition.
 */
static void arco_addb_swap(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t id1 = argv[1]->i;
    int32_t id2 = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Addb, addb, id, "arco_addb_swap");
    ANY_UGEN_FROM_ID(ugen, id1, "arco_addb_swap");
    ANY_UGEN_FROM_ID(replacement, id2, "arco_addb_swap");
    if (replacement->rate != 'b') {
        arco_warn("/arco/addb/swap id2 (%d) is not block rate\n", id2);
        return;
    }
    addb->swap(ugen, replacement);
    printf("arco_addb_swap: removed %d, inserted %d in output set\n", id, id2);
}



static void addb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/addb/new", "ii", arco_addb_new, NULL, true, true);
    o2sm_method_new("/arco/addb/ins", "ii", arco_addb_ins, NULL, true, true);
    o2sm_method_new("/arco/addb/rem", "ii", arco_addb_rem, NULL, true, true);
    o2sm_method_new("/arco/addb/swap", "iii", arco_addb_swap, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer addb_init_obj(addb_init);
