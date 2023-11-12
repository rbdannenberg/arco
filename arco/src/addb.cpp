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



static void addb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/addb/new", "ii", arco_addb_new, NULL, true, true);
    o2sm_method_new("/arco/addb/ins", "ii", arco_addb_ins, NULL, true, true);
    o2sm_method_new("/arco/addb/rem", "ii", arco_addb_rem, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer addb_init_obj(addb_init);
