/* o2audioio.cpp -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#include "const.h"
#include "audioblock.h"
#include "o2audioio.h"

const char *O2audioio_name = "O2audioio";


void send_o2audioio_play(int64_t addr, bool play_flag)
{
    // o2sm_send_cmd("/fileio/o2audioio/play", 0, "hB", addr, play_flag);
    o2_send_start();
    o2_add_int64(addr);
    o2_add_bool(play_flag);
    O2message_ptr msg =
            o2_message_finish(0.0, "/fileio/o2audioio/play", true);
    fileio_bridge->outgoing.push((O2list_elem *) msg);
}
    

O2audioio::~O2audioio()
{
    if (!stopped) {
        send_o2audioio_play((int64_t) this, false);
    }
}


/* O2SM INTERFACE: /arco/o2audioio/new
       int32 id,
       string recvaddr,
       int32 recvchans,
       string destaddr,
       int32 sendchans,
       int32 buffsize,
       int32 sampletype,
       int32 msgsize
 */
void arco_o2audioio_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *recvaddr = argv[1]->s;
    int32_t recvchans =  argv[2]->i;
    char *destaddr = argv[3]->s;
    int32_t sendchans = argv[4]->i;
    int32_t buffsize = argv[5]->i;
    int32_t sampletype = argv[6]->i;
    int32_t msgsize = argv[7]->i;
    // end unpack message

    new O2audioio(id, recvaddr, recvchans, destaddr, sendchans, buffsize, sampletype, msgsize);
}


/* O2SM INTERFACE: /arco/o2audioio/samps int64 addr, int64 ablock;
 */
void arco_o2audioio_samps(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int64_t ablock = argv[1]->h;
    // end unpack message

    O2audioio *o2audioio = (O2audioio *) addr;
    o2audioio->samps((Audioblock *) ablock);
}


/* O2SM INTERFACE: /arco/o2audioio/ready int64 addr, int32 chans, bool ready;
 */
void arco_o2audioio_ready(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int32_t chans = argv[1]->i;
    bool ready = argv[2]->B;
    // end unpack message

    O2audioio *o2audioio = (O2audioio *) addr;
    o2audioio->ready(ready);
}


/* O2SM INTERFACE: /arco/o2audioio/play int32 id, bool play;
 */
void arco_o2audioio_play(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool play = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2audioio_play");
    o2audioio->play(play);
}


/* O2SM INTERFACE: /arco/o2aud/play int32 id, bool play;
 */
void arco_o2aud_play(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool play = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2aud_play");
    o2audioio->play(play);
}


/* O2SM INTERFACE: /arco/o2aud/data int32 id, double when, 
                                    int32 drop, blob samps;
 */
void arco_o2aud_data(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    double when = argv[1]->d;
    // end unpack message

    UGEN_FROM_ID(O2audioio, o2audioio, id, "arco_o2aud_data");
    o2audioio->data(when, drop, samps);
}


static void o2audioio_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/o2aud/data", "idib", arco_o2aud_data,
                    NULL, true, true);
    o2sm_method_new("/arco/o2aud/play", "ii", arco_o2aud_play,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer o2audioio_init_obj(o2audioio_init);

