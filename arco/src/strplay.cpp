/* strplay.cpp -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#include "const.h"
#include "audioblock.h"
#include "strplay.h"

const char *Strplay_name = "Strplay";

void send_strplay_play(int64_t addr, bool play_flag)
{
    // o2sm_send_cmd("/fileio/strplay/play", 0, "hB", addr, play_flag);
    o2_send_start();
    o2_add_int64(addr);
    o2_add_bool(play_flag);
    O2message_ptr msg =
            o2_message_finish(0.0, "/fileio/strplay/play", true);
    fileio_bridge->outgoing.push((O2list_elem *) msg);
}
    

Strplay::~Strplay()
{
    if (!stopped) {
        send_strplay_play((int64_t) this, false);
    }
    if (action_id) send_action_id(action_id);
}


/* O2SM INTERFACE: /arco/strplay/new
       int32 id,
       int32 channels,
       string filename,
       float start,
       float end,
       bool cycle,
       bool mix,
       bool expand;
 */
void arco_strplay_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t channels = argv[1]->i;
    char *filename = argv[2]->s;
    float start = argv[3]->f;
    float end = argv[4]->f;
    bool cycle = argv[5]->B;
    bool mix = argv[6]->B;
    bool expand = argv[7]->B;
    // end unpack message

    new Strplay(id, filename, channels, start, end, cycle, mix, expand);
}


/* O2SM INTERFACE: /arco/strplay/samps int64 addr, int64 ablock;
 */
void arco_strplay_samps(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int64_t ablock = argv[1]->h;
    // end unpack message

    Strplay *strplay = (Strplay *) addr;
    strplay->samps((Audioblock *) ablock);
}


/* O2SM INTERFACE: /arco/strplay/ready int64 addr, int32 chans, bool ready;
 */
void arco_strplay_ready(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int32_t chans = argv[1]->i;
    bool ready = argv[2]->B;
    // end unpack message

    Strplay *strplay = (Strplay *) addr;
    strplay->ready(ready);
}


/* O2SM INTERFACE: /arco/strplay/play int32 id, bool play;
 */
void arco_strplay_play(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool play = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(Strplay, strplay, id, "arco_strplay_play");
    strplay->play(play);
}


/* O2SM INTERFACE: /arco/strplay/act int32 id, int32 action_id;
 *    set the action_id
 */
void arco_strplay_act(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t action_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Strplay, strplay, id, "arco_strplay_act");
    strplay->action_id = action_id;
    printf("arco_strplay_act: set ugen %d action_id %d\n", id, action_id);
}


static void strplay_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/strplay/new", "iisffBBB", arco_strplay_new, NULL, true, true);
    o2sm_method_new("/arco/strplay/samps", "hh", arco_strplay_samps, NULL, true, true);
    o2sm_method_new("/arco/strplay/ready", "hiB", arco_strplay_ready, NULL, true, true);
    o2sm_method_new("/arco/strplay/play", "iB", arco_strplay_play, NULL, true, true);
    o2sm_method_new("/arco/strplay/act", "ii", arco_strplay_act, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer strplay_init_obj(strplay_init);
