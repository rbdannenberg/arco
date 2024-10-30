/* tableoscb.cpp - simple table-lookup oscillator
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

#include "arcougen.h"
#include "wavetables.h"
#include "tableoscb.h"

const char *Tableoscb_name = "Tableoscb";

/* O2SM INTERFACE: /arco/tableoscb/new 
        int32 id, int32 chans, int32 freq, int32 amp;
 */
void arco_tableoscb_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t freq = argv[2]->i;
    int32_t amp = argv[3]->i;
    float phase = argv[3]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(freq_ugen, freq, "arco_tableoscb_new");
    ANY_UGEN_FROM_ID(amp_ugen, amp, "arco_tableoscb_new");
    new Tableoscb(id, chans, freq_ugen, amp_ugen, phase);
}
    

/* O2SM INTERFACE: /arco/tableoscb/repl_freq int32 id, int32 freq_id;
 */
static void arco_tableoscb_repl_freq(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t freq_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_repl_freq");
    ANY_UGEN_FROM_ID(freq, freq_id, "arco_tableoscb_repl_freq");
    tableoscb->repl_freq(freq);
}


/* O2SM INTERFACE: /arco/tableoscb/set_freq int32 id, int32 chan, float freq;
 */
void arco_tableoscb_set_freq(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float freq = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_set_freq");
    tableoscb->set_freq(chan, freq);
}


/* O2SM INTERFACE: /arco/tableoscb/repl_amp int32 id, int32 amp_id;
 */
static void arco_tableoscb_repl_amp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t amp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_repl_amp");
    ANY_UGEN_FROM_ID(amp, amp_id, "arco_tableoscb_repl_amp");
    tableoscb->repl_amp(amp);
}


/* O2SM INTERFACE: /arco/tableoscb/set_amp int32 id, int32 chan, float amp;
 */
void arco_tableoscb_set_amp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float amp = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_set_amp");
    tableoscb->set_amp(chan, amp);
}


/* O2SM INTERFACE: /arco/tableoscb/sel int32 id, int32 index;
 */
void arco_tableoscb_sel(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_sel");
    tableoscb->select(index);
}


void arco_tableoscb_createtas(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int32_t tlen = argv[2]->i;
    int slen = argv[3]->v.len;
    float *data = argv[3]->v.vf;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_createtas");
    tableoscb->create_tas(index, tlen, slen, data);
}


void arco_tableoscb_createtcs(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int32_t tlen = argv[2]->i;
    int slen = argv[3]->v.len;
    float *data = argv[3]->v.vf;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_createtcs");
    tableoscb->create_tas(index, tlen, slen, data);
}


void arco_tableoscb_createttd(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int tlen = argv[2]->v.len;
    float *data = argv[2]->v.vf;
    // end unpack message

    UGEN_FROM_ID(Tableoscb, tableoscb, id, "arco_tableoscb_createttd");
    tableoscb->create_ttd(index, tlen, data);
}


static void tableoscb_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/tableoscb/new", "iiiif", arco_tableoscb_new, NULL,
                    true, true);
    o2sm_method_new("/arco/tableoscb/repl_freq", "ii",
                    arco_tableoscb_repl_freq, NULL, true, true);
    o2sm_method_new("/arco/tableoscb/set_freq", "iif",
                    arco_tableoscb_set_freq, NULL, true, true);
    o2sm_method_new("/arco/tableoscb/repl_amp", "ii",
                    arco_tableoscb_repl_amp, NULL, true, true);
    o2sm_method_new("/arco/tableoscb/set_amp", "iif",
                    arco_tableoscb_set_amp, NULL, true, true);
    o2sm_method_new("/arco/tableoscb/sel", "ii", arco_tableoscb_sel,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
    o2sm_method_new("/arco/tableoscb/createtas", "iiivf",
                    arco_tableoscb_createtas, NULL, true, true);
    o2sm_method_new("/arco/tableoscb/createtcs", "iiivf",
                    arco_tableoscb_createtcs, NULL, true, true);
    o2sm_method_new("/arco/tableoscb/createttd", "iivf",
                    arco_tableoscb_createttd, NULL, true, true);
}

Initializer tableoscb_init_obj(tableoscb_init);
