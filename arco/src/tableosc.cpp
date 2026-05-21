/* tableosc.cpp - simple table-lookup oscillator
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

#include "arcougen.h"
#include "wavetables.h"
#include "tableosc.h"

const char *Tableosc_name = "Tableosc";

/* O2SM INTERFACE: /arco/tableosc/new 
       int32 id, int32 chans, int32 freq, int32 amp, float phase;
 */
void arco_tableosc_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t freq = argv[2]->i;
    int32_t amp = argv[3]->i;
    int32_t phase = argv[3]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(freq_ugen, freq, "arco_tableosc_new");
    ANY_UGEN_FROM_ID(amp_ugen, amp, "arco_tableosc_new");
    new Tableosc(id, chans, freq_ugen, amp_ugen, phase);
}
    

/* O2SM INTERFACE: /arco/tableosc/repl_freq int32 id, int32 freq_id;
 */
static void arco_tableosc_repl_freq(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t freq_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_repl_freq");
    ANY_UGEN_FROM_ID(freq, freq_id, "arco_tableosc_repl_freq");
    tableosc->repl_freq(freq);
}


/* O2SM INTERFACE: /arco/tableosc/set_freq int32 id, int32 chan, float freq;
 */
void arco_tableosc_set_freq(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float freq = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_set_freq");
    tableosc->set_freq(chan, freq);
}


/* O2SM INTERFACE: /arco/tableosc/repl_amp int32 id, int32 amp_id;
 */
static void arco_tableosc_repl_amp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t amp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_repl_amp");
    ANY_UGEN_FROM_ID(amp, amp_id, "arco_tableosc_repl_amp");
    tableosc->repl_amp(amp);
}


/* O2SM INTERFACE: /arco/tableosc/set_amp int32 id, int32 chan, float amp;
 */
void arco_tableosc_set_amp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float amp = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_set_amp");
    tableosc->set_amp(chan, amp);
}


/* O2SM INTERFACE: /arco/tableosc/set_phase int32 id, int32 chan, float phase;
 */
void arco_tableosc_set_phase(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float phase = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_set_phase");
    tableosc->set_phase(chan, phase);
}


/* O2SM INTERFACE: /arco/tableosc/sel int32 id, int32 index;
 */
void arco_tableosc_sel(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_sel");
    tableosc->select(index);
}


/* O2SM INTERFACE: /arco/tableosc/borrow int32 id, int32 lender;
 */
void arco_tableosc_borrow(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t lender = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_borrow");
    UGEN_FROM_ID(Tableosc, wtugen, lender, "arco_tableosc_borrow");
    Wavetables *lender_ugen = (Wavetables *) wtugen;
    

    if (lender_ugen && streql(lender_ugen->classname(), "Tableosc")) {
        tableosc->borrow(lender_ugen);
    } else {
        arco_warn("Tableosc borrow ignored: lender %d is not a Tableosc",
                  lender);
    }
}

void arco_tableosc_createtas(O2SM_HANDLER_ARGS)
{
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int32_t tlen = argv[2]->i;
    int slen = argv[3]->v.len;
    float *data = argv[3]->v.vf;

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_createtas");
    tableosc->create_tas(index, tlen, slen, data);
}


void arco_tableosc_createtcs(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int32_t tlen = argv[2]->i;
    int slen = argv[3]->v.len;
    float *data = argv[3]->v.vf;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_createtcs");
    tableosc->create_tas(index, tlen, slen, data);
}


void arco_tableosc_createttd(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t index = argv[1]->i;
    int tlen = argv[2]->v.len;
    float *data = argv[2]->v.vf;
    // end unpack message

    UGEN_FROM_ID(Tableosc, tableosc, id, "arco_tableosc_createttd");
    tableosc->create_ttd(index, tlen, data);
}


static void tableosc_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/tableosc/new", "iiiif", arco_tableosc_new, NULL,
                    true, true);
    o2sm_method_new("/arco/tableosc/repl_freq", "ii",
                    arco_tableosc_repl_freq, NULL, true, true);
    o2sm_method_new("/arco/tableosc/set_freq", "iif",
                    arco_tableosc_set_freq, NULL, true, true);
    o2sm_method_new("/arco/tableosc/repl_amp", "ii", arco_tableosc_repl_amp,
                    NULL, true, true);
    o2sm_method_new("/arco/tableosc/set_amp", "iif", arco_tableosc_set_amp,
                    NULL, true, true);
    o2sm_method_new("/arco/tableosc/set_phase", "iif",
                    arco_tableosc_set_phase, NULL, true, true);
    o2sm_method_new("/arco/tableosc/sel", "ii", arco_tableosc_sel, NULL,
                    true, true);
    o2sm_method_new("/arco/tableosc/borrow", "ii", arco_tableosc_borrow,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
    o2sm_method_new("/arco/tableosc/createtas", "iiivf",
                    arco_tableosc_createtas, NULL, true, true);
    o2sm_method_new("/arco/tableosc/createtcs", "iiivf",
                    arco_tableosc_createtcs, NULL, true, true);
    o2sm_method_new("/arco/tableosc/createttd", "iivf",
                    arco_tableosc_createttd, NULL, true, true);
}

Initializer tableosc_init_obj(tableosc_init);
