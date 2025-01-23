/* thru.cpp -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"
#include "thru.h"

const char *Thru_name = "Thru";

/* O2SM INTERFACE: /arco/thru/new int32 id, int32 chans, int32 input_id;
 */
void arco_thru_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input_id = argv[2]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(input, input_id, "arco_thru_new");
    new Thru(id, chans, input);
}


/* O2SM INTERFACE: /arco/thru/repl_input int32 id, int32 input_id;
 */
void arco_thru_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Thru, thru, id, "arco_thru_repl_input")
    ANY_UGEN_FROM_ID(input, input_id, "arco_thru_repl_input");
    thru->repl_input(input);
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
    o2sm_method_new("/arco/thru/repl_input", "ii", arco_thru_repl_input, NULL,
                    true, true);
    o2sm_method_new("/arco/thru/alt", "ii", arco_thru_alt, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer thru_init_obj(thru_init);
