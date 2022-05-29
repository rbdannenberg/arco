/* thru.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "thru.h"

const char *Thru_name = "Thru";

/* O2SM INTERFACE: /arco/thru/new int32 id, int32 chans, int32 inp;
 */
void arco_thru_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, inp, "arco_thru_new");
    new Thru(id, chans, ugen);
}


static void thru_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/thru/new", "iii", arco_thru_new, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer thru_init_obj(thru_init);
