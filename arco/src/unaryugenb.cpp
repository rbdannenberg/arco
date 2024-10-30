/* unaryb -- unit generator for arco
 *
 */

#include "arcougen.h"
#include "fastrand.h"
#include "unaryugenb.h"

const char *Unaryb_name = "Unaryb";

/* O2SM INTERFACE: /arco/unaryb/new int32 id, int32 chans, 
                                  int32 op, int32 x1, int32 x2;
 */
void arco_unaryb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t op = argv[2]->i;
    int32_t x1 = argv[3]->i;
    int32_t x2 = argv[4]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(x1_ugen,x1, "arco_unaryb_new");
    ANY_UGEN_FROM_ID(x2_ugen,x2, "arco_unaryb_new");

    new Unaryb(id, chans, op, x1_ugen);
}


/* O2SM INTERFACE: /arco/unaryb/repl_x1 int32 id, int32 x1_id;
 */
static void arco_unaryb_repl_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t x1_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Unaryb, unaryb, id, "arco_unaryb_repl_x1");
    ANY_UGEN_FROM_ID(x1, x1_id, "arco_unaryb_repl_x1");
    unaryb->repl_x1(x1);
}


/* O2SM INTERFACE: /arco/unaryb/set_x1 int32 id, int32 chan, float val;
 */
static void arco_unaryb_set_x1(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Unaryb, unaryb, id, "arco_unaryb_set_x1");
    unaryb->set_x1(chan, val);
}


static void unaryb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/unaryb/new", "iiiii", arco_unaryb_new, NULL,
                    true, true);
    o2sm_method_new("/arco/unaryb/repl_x1", "ii", arco_unaryb_repl_x1, NULL,
                    true, true);
    o2sm_method_new("/arco/unaryb/set_x1", "iif", arco_unaryb_set_x1, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION

    // class initialization code from faust:
}

Initializer unaryb_init_obj(unaryb_init);
