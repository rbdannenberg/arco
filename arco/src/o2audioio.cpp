/* o2audioio.cpp -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#include "const.h"
#include "blockqueue.h"
#include "o2audioio.h"

const char *O2audioio_name = "O2audioio";

/* O2SM INTERFACE: /arco/o2aud/new
       int32 id,
       int32 input,
       string destaddr,
       int32 destchans,
       int32 recvchans,
       int32 buffsize,
       int32 sampletype,
       int32 msgsize;
 */
void arco_o2audioio_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input = argv[1]->i;
    char *destaddr = argv[2]->s;
    int32_t destchans = argv[3]->i;
    int32_t recvchans = argv[4]->i;
    int32_t buffsize = argv[5]->i;
    int32_t sampletype = argv[6]->i;
    int32_t msgsize = argv[7]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(input_ugen, input, "arco_o2audioio_new");

    new O2audioio(id, recvchans, input_ugen, destaddr, destchans, buffsize,
                  sampletype, msgsize);
}


/* O2SM INTERFACE: /arco/o2aud/repl_input int32 id, int32 input_id;
 */
static void arco_o2aud_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2aud_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_o2aud_repl_input");
    o2audioio->repl_input(input);
}


/* O2SM INTERFACE: /arco/o2aud/data int32 id, double when,
                                    int32 drop, blob samps;
 */
void arco_o2audioio_data(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    double when = argv[1]->d;
    int32_t drop = argv[2]->i;
    O2blob_ptr samps = &argv[3]->b;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2audioio_data");
    o2audioio->data(when, drop, samps);
}


/* O2SM INTERFACE: /arco/o2aud/enab int32 id, bool enab;
 */
void arco_o2audioio_enab(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool enab = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2audioio_enab");
    o2audioio->enable(enab);
}


/* O2SM INTERFACE: /arco/o2aud/data int32 id, double when, 
                                    int32 drop, blob samps;
 */
void arco_o2aud_data(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    double when = argv[1]->d;
    int32_t drop = argv[2]->i;
    O2blob_ptr samps = &argv[3]->b;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2aud_data");
    o2audioio->data(when, drop, samps);
}


static void o2audioio_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/o2aud/new", "iisiiiii", arco_o2audioio_new, NULL,
                    true, true);
    o2sm_method_new("/arco/o2aud/repl_input", "ii", arco_o2aud_repl_input,
                    NULL, true, true);
    o2sm_method_new("/arco/o2aud/data", "idib", arco_o2audioio_data, NULL,
                    true, true);
    o2sm_method_new("/arco/o2aud/enab", "iB", arco_o2audioio_enab, NULL,
                    true, true);
    o2sm_method_new("/arco/o2aud/data", "idib", arco_o2aud_data, NULL, true,
                    true);
    // END INTERFACE INITIALIZATION
}


Initializer o2audioio_init_obj(o2audioio_init);

