/* dualslewb -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2022
 */

#include "arcougen.h"
#include "dualslewb.h"

const char *Dualslewb_name = "Dualslewb";

/* O2SM INTERFACE: /arco/dualslewb/new int32 id, int32 chans, 
            int32 input, float attack, float release, float current,
            int32 attack_linear, int32 release_linear;
 */
void arco_dualslewb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input = argv[2]->i;
    float attack = argv[3]->f;
    float release = argv[4]->f;
    float current = argv[5]->f;
    int32_t attack_linear = argv[6]->i;
    int32_t release_linear = argv[7]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(input_ugen, input, "arco_dualslewb_new");

    new Dualslewb(id, chans, input_ugen, attack, release, current,
                  attack_linear, release_linear);
}


/* O2SM INTERFACE: /arco/dualslewb/repl_input int32 id, int32 input_id;
 */
static void arco_dualslewb_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Dualslewb, dualslewb, id, "arco_dualslewb_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_dualslewb_repl_input");
    dualslewb->repl_input(input);
}


/* O2SM INTERFACE: /arco/dualslewb/attack int32 id, float attack, 
                                          int32 attack_linear;
 */
void arco_dualslewb_attack(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float attack = argv[1]->f;
    int32_t attack_linear = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Dualslewb, dualslewb, id, "arco_dualslewb_attack");
    dualslewb->set_attack(attack, attack_linear != 0);
}


/* O2SM INTERFACE: /arco/dualslewb/release int32 id, float release, 
                                           int32 release_linear;
 */
void arco_dualslewb_release(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float release = argv[1]->f;
    int32_t release_linear = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Dualslewb, dualslewb, id, "arco_dualslewb_release");
    dualslewb->set_release(release, release_linear != 0);
}


/* O2SM INTERFACE: /arco/dualslewb/set_current int32 id, int32 chan, 
                                               float current;
 */
void arco_dualslewb_current(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float current = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Dualslewb, dualslewb, id, "arco_dualslewb_current");
    dualslewb->set_current(current, chan);
}




static void dualslewb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/dualslewb/new", "iiifffii", arco_dualslewb_new,
                    NULL, true, true);
    o2sm_method_new("/arco/dualslewb/repl_input", "ii",
                    arco_dualslewb_repl_input, NULL, true, true);
    o2sm_method_new("/arco/dualslewb/attack", "ifi", arco_dualslewb_attack,
                    NULL, true, true);
    o2sm_method_new("/arco/dualslewb/release", "ifi", arco_dualslewb_release,
                    NULL, true, true);
    o2sm_method_new("/arco/dualslewb/set_current", "iif",
                    arco_dualslewb_current, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer dualslewb_init_obj(dualslewb_init);
