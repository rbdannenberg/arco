// yin.cpp -- yin unit generator for pitch estimation
//
// Roger B. Dannenberg
// Aug 2023

/* yin is an audio unit generator that sends messages with
   pitch estimates.
*/

#include <cmath>
#include "arcougen.h"
#include "windowedinput.h"
#include "yin.h"

const char *Yin_name = "Yin";


/* O2SM INTERFACE: /arco/yin/new int32 id, int32 chans, int32 inp_id, 
      int32 minstep, int32 maxstep, int32 hopsize, string reply_addr;
 */
void arco_yin_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp_id = argv[2]->i;
    int32_t minstep = argv[3]->i;
    int32_t maxstep = argv[4]->i;
    int32_t hopsize = argv[5]->i;
    char *reply_addr = argv[6]->s;
    // end unpack message

    ANY_UGEN_FROM_ID(inp, inp_id, "arco_yin_new");
    new Yin(id, chans, inp, minstep, maxstep, hopsize, reply_addr);
}


/* O2SM INTERFACE: /arco/yin/repl_inp int32 id, int32 inp_id;
 */
void arco_yin_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Yin, yin, id, "arco_yin_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_yin_repl_inp");
    yin->repl_inp(inp);
}


static void yin_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/yin/new", "iiiiiis", arco_yin_new, NULL, true,
                     true);
    o2sm_method_new("/arco/yin/repl_inp", "ii", arco_yin_repl_inp, NULL, true,
                     true);
    // END INTERFACE INITIALIZATION
}


Initializer yin_init_obj(yin_init);



