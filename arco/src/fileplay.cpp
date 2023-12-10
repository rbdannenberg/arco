/* fileplay.cpp -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "arcougen.h"
#include "o2atomic.h"
#include "sharedmem.h"
#include "const.h"
#include "audioblock.h"
#include "fileplay.h"

const char *Fileplay_name = "Fileplay";

void send_fileplay_play(int64_t addr, bool play_flag)
{
    // o2sm_send_cmd("/fileio/fileplay/play", 0, "hB", addr, play_flag);
    o2_send_start();
    o2_add_int64(addr);
    o2_add_bool(play_flag);
    O2message_ptr msg =
            o2_message_finish(0.0, "/fileio/fileplay/play", true);
    fileio_bridge->outgoing.push((O2list_elem *) msg);
}
    

Fileplay::~Fileplay()
{
    if (!stopped) {
        send_fileplay_play((int64_t) this, false);
    }
}


/* O2SM INTERFACE: /arco/fileplay/new
       int32 id,
       int32 channels,
       string filename,
       float start,
       float end,
       bool cycle,
       bool mix,
       bool expand;
 */
void arco_fileplay_new(O2SM_HANDLER_ARGS)
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

    new Fileplay(id, filename, channels, start, end, cycle, mix, expand);
}


/* O2SM INTERFACE: /arco/fileplay/samps int64 addr, int64 ablock;
 */
void arco_fileplay_samps(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int64_t ablock = argv[1]->h;
    // end unpack message

    Fileplay *fileplay = (Fileplay *) addr;
    fileplay->samps((Audioblock *) ablock);
}


/* O2SM INTERFACE: /arco/fileplay/ready int64 addr, int32 chans, bool ready;
 */
void arco_fileplay_ready(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int32_t chans = argv[1]->i;
    bool ready = argv[2]->B;
    // end unpack message

    Fileplay *fileplay = (Fileplay *) addr;
    fileplay->ready(ready);
}


/* O2SM INTERFACE: /arco/fileplay/play int32 id, bool play;
 */
void arco_fileplay_play(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    bool play = argv[1]->B;
    // end unpack message

    UGEN_FROM_ID(Fileplay, fileplay, id, "arco_fileplay_play");
    fileplay->play(play);
}


static void fileplay_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/fileplay/new", "iisffBBB", arco_fileplay_new, NULL, true, true);
    o2sm_method_new("/arco/fileplay/samps", "hh", arco_fileplay_samps, NULL, true, true);
    o2sm_method_new("/arco/fileplay/ready", "hiB", arco_fileplay_ready, NULL, true, true);
    o2sm_method_new("/arco/fileplay/play", "iB", arco_fileplay_play, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer fileplay_init_obj(fileplay_init);
