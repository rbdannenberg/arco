/* thru.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "audioio.h"
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


/* O2SM INTERFACE: /arco/thru/repl_inp int32 id, int32 inp;
 */
void arco_thru_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Thru, thru, id, "arco_thru_repl_inp")
    ANY_UGEN_FROM_ID(ugen, inp, "arco_thru_repl_inp");
    thru->repl_inp(ugen);
}


/* O2SM INTERFACE: /arco/thru/alt int32 id, int32 alt_id;
 */
void arco_thru_alt(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t alt_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Thru, thru, id, "arco_thru_alt")
    ANY_UGEN_FROM_ID(alt, alt_id, "arco_thru_alt");
    thru->set_alternate(alt);
}


static void thru_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/thru/new", "iii", arco_thru_new, NULL, true, true);
    o2sm_method_new("/arco/thru/repl_inp", "ii", arco_thru_repl_inp, NULL, true, true);
    o2sm_method_new("/arco/thru/alt", "ii", arco_thru_alt, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer thru_init_obj(thru_init);
