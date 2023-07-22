/* flsyn.cpp - fluidsynth interface for Arco
 *
 * Roger B. Dannenberg
 */

#include "arcougen.h"
#include "flsyn.h"

const char *Flsyn_name = "Flsyn";


/* O2SM INTERFACE: /arco/flsyn/new int32 id, string path;
 */
void arco_flsyn_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *path = argv[1]->s;
    // end unpack message

    new Flsyn(id, path);
}


/* O2SM INTERFACE: /arco/flsyn/off int32 id, int32 chan;
 */
void arco_flsyn_off(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_off");
    flsyn->all_off(chan);
}


/* O2SM INTERFACE: /arco/flsyn/cc int32 id, int32 chan, 
                                  int32 num, int32 val;
 */
void arco_flsyn_cc(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t num = argv[2]->i;
    int32_t val = argv[3]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_cc");
    flsyn->control_change(chan, num, val);
}


/* O2SM INTERFACE: /arco/flsyn/cp int32 id, int32 chan, int32 val;
 */
void arco_flsyn_cp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t val = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_cp");
    flsyn->channel_pressure(chan, val);
}


/* O2SM INTERFACE: /arco/flsyn/kp int32 id, int32 chan, 
                                  int32 key, int32 val;
 */
void arco_flsyn_kp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t key = argv[2]->i;
    int32_t val = argv[3]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_kp");
    flsyn->key_pressure(chan, key, val);
}


/* O2SM INTERFACE: /arco/flsyn/noteoff int32 id, int32 chan, int32 key;
 */
void arco_flsyn_noteoff(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t key = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_noteoff");
    flsyn->noteoff(chan, key);
}


/* O2SM INTERFACE: /arco/flsyn/noteon int32 id, int32 chan,
                                      int32 key, int32 vel;
 */
void arco_flsyn_noteon(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t key = argv[2]->i;
    int32_t vel = argv[3]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_noteon");
    flsyn->noteon(chan, key, vel);
}


/* O2SM INTERFACE: /arco/flsyn/pbend int32 id, int32 chan, float val;
 */
void arco_flsyn_pbend(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_pbend");
    flsyn->pitchbend(chan, val);
}


/* O2SM INTERFACE: /arco/flsyn/psens int32 id, int32 chan, int32 val;
 */
void arco_flsyn_psens(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t val = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_psens");
    flsyn->pitchsens(chan, val);
}


/* O2SM INTERFACE: /arco/flsyn/prog int32 id, int32 chan, int32 program;
 */
void arco_flsyn_prog(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    int32_t program = argv[2]->i;
    // end unpack message

    UGEN_FROM_ID(Flsyn, flsyn, id, "arco_flsyn_prog");
    flsyn->program_change(chan, program);
}


static void flsyn_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/flsyn/new", "is", arco_flsyn_new, NULL, true, true);
    o2sm_method_new("/arco/flsyn/off", "ii", arco_flsyn_off, NULL, true, true);
    o2sm_method_new("/arco/flsyn/cc", "iiii", arco_flsyn_cc, NULL, true, true);
    o2sm_method_new("/arco/flsyn/cp", "iii", arco_flsyn_cp, NULL, true, true);
    o2sm_method_new("/arco/flsyn/kp", "iiii", arco_flsyn_kp, NULL, true, true);
    o2sm_method_new("/arco/flsyn/noteoff", "iii", arco_flsyn_noteoff, NULL,
                     true, true);
    o2sm_method_new("/arco/flsyn/noteon", "iiii", arco_flsyn_noteon, NULL,
                     true, true);
    o2sm_method_new("/arco/flsyn/pbend", "iif", arco_flsyn_pbend, NULL, true,
                     true);
    o2sm_method_new("/arco/flsyn/psens", "iii", arco_flsyn_psens, NULL, true,
                     true);
    o2sm_method_new("/arco/flsyn/prog", "iii", arco_flsyn_prog, NULL, true,
                     true);
    // END INTERFACE INITIALIZATION
}


Initializer flsyn_init_obj(flsyn_init);
