/* fader -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Nov 2023
 *
 * fader combines a 1-segment envelope with a multiply to form 
 * a smooth gain control. This implementation is based on Multx.
 *
 */

#include "arcougen.h"
#include "fader.h"

const char *Fader_name = "Fader";


/* O2SM INTERFACE: /arco/fader/new int32 id, int32 chans, int32 input,
                                   float current, int32 mode;
 */
void arco_fader_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input = argv[2]->i;
    float current = argv[3]->f;
    int32_t mode = argv[4]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(input_ugen, input, "arco_fader_new");
    new Fader(id, chans, input_ugen, current, mode);
}


/* O2SM INTERFACE: /arco/fader/repl_input int32 id, int32 input_id;
 */
static void arco_fader_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Fader, fader, id, "arco_fader_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_fader_repl_x1");
    fader->repl_input(input);
}


/* O2SM INTERFACE: /arco/fader/cur int32 id, int32 chan, float val;
 */
static void arco_fader_cur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Fader, fader, id, "arco_fader_cur");
    fader->set_current(chan, val);
}


/* O2SM INTERFACE: /arco/fader/dur int32 id, float dur;
 */
static void arco_fader_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float dur = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Fader, fader, id, "arco_fader_dur");
    fader->set_dur(dur);
}


/* O2SM INTERFACE: /arco/fader/goal int32 id, int chan, float goal;
 */
static void arco_fader_goal(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float goal = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Fader, fader, id, "arco_fader_goal");
    fader->set_goal(chan, goal);
}


/* O2SM INTERFACE: /arco/fader/mode int32 id, int32 mode;
 */
static void arco_fader_mode(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t mode = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Fader, fader, id, "arco_fader_repl_x2");
    fader->set_mode(mode);
}


static void fader_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/fader/new", "iiifi", arco_fader_new, NULL, true,
                    true);
    o2sm_method_new("/arco/fader/repl_input", "ii", arco_fader_repl_input,
                    NULL, true, true);
    o2sm_method_new("/arco/fader/cur", "iif", arco_fader_cur, NULL,
                    true, true);
    o2sm_method_new("/arco/fader/dur", "if", arco_fader_dur, NULL,
                    true, true);
    o2sm_method_new("/arco/fader/goal", "iif", arco_fader_goal, NULL,
                    true, true);
    o2sm_method_new("/arco/fader/mode", "ii", arco_fader_mode, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION

    // class initialization code from faust:
}

Initializer fader_init_obj(fader_init);
