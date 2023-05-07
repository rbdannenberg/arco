/* filerec.cpp -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * May 2022
 */

#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#include "const.h"
#include "audioblock.h"
#include "filerec.h"

const char *Filerec_name = "Filerec";


Filerec::~Filerec()
{
    inp->unref();
}


/* O2SM INTERFACE: /arco/filerec/new
       int32 id,
       int32 channels,
       string filename,
       int32 inp_id;
 */
void arco_filerec_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t channels = argv[1]->i;
    char *filename = argv[2]->s;
    int32_t inp_id = argv[3]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(inp, inp_id, "arco_filerec_new");
    new Filerec(id, channels, filename, inp);
}


/* O2SM INTERFACE: /arco/filerec/repl_inp int32 id, int32 inp_id;
 */
void arco_filerec_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Filerec, filerec, id, "arco_filerec_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_filerec_repl_inp");

    filerec->repl_inp(inp);
}


/* O2SM INTERFACE: /arco/filerec/samps int64 addr;
 */
void arco_filerec_samps(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    // end unpack message

    Filerec *filerec = (Filerec *) addr;
    filerec->samps();
}


/* O2SM INTERFACE: /arco/filerec/ready int64 addr, bool ready_flag;
 */
void arco_filerec_ready(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    bool ready_flag = argv[1]->B;
    // end unpack message

    Filerec *filerec = (Filerec *) addr;
    filerec->ready(ready_flag);
}


/* O2SM INTERFACE: /arco/filerec/rec int32 id, bool rec;
 */
void arco_filerec_rec(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool rec = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(Filerec, filerec, id, "arco_filerec_rec");
    filerec->record(rec);
}


/* O2SM INTERFACE: /arco/filerec/act int32 id, int32 action_id;
 *    set the action_id
 */
void arco_filerec_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Filerec, filerec, id, "arco_filerec_act");
    filerec->send_action_id(action_id);
    printf("arco_filerec_act: set ugen %d action_id %d\n", id, action_id);
}


static void filerec_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/filerec/new", "iisi", arco_filerec_new, NULL, true, true);
    o2sm_method_new("/arco/filerec/repl_inp", "ii", arco_filerec_repl_inp, NULL, true, true);
    o2sm_method_new("/arco/filerec/samps", "h", arco_filerec_samps, NULL, true, true);
    o2sm_method_new("/arco/filerec/ready", "hB", arco_filerec_ready, NULL, true, true);
    o2sm_method_new("/arco/filerec/rec", "iB", arco_filerec_rec, NULL, true, true);
    o2sm_method_new("/arco/filerec/act", "ii", arco_filerec_act, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer filerec_init_obj(filerec_init);
