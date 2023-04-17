/* strplay.cpp -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include "arcougen.h"
#include "const.h"
#include "audioblock.h"
#include "strplay.h"

const char *Strplay_name = "Strplay";

Strplay::~Strplay()
{
    if (!stopped) {
        o2sm_send_cmd("/fileio/strplay/stop", 0, "i", id);
    }
}


/* O2SM INTERFACE: /arco/strplay/new
       int32 id, 
       string filename,
       int32 channels,
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
    char *filename = argv[1]->s;
    int32_t channels = argv[2]->i;
    float start = argv[3]->f;
    float end = argv[4]->f;
    bool cycle = argv[5]->B;
    bool mix = argv[6]->B;
    bool expand = argv[7]->B;
    // end unpack message

    new Strplay(id, filename, channels, start, end, cycle, mix, expand);
}


/* O2SM INTERFACE: /arco/strplay/samps int32 id, int64 ablock;
 */
void arco_strplay_samps(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int64_t ablock = argv[1]->h;
    // end unpack message

    UGEN_FROM_ID(Strplay, strplay, id, "arco_strplay_samps");
    strplay->samps((Audioblock *) ablock);
}


/* O2SM INTERFACE: /arco/strplay/ready int32 id, int32 chans, bool ready;
 */
void arco_strplay_ready(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    bool ready = argv[2]->B;
    // end unpack message

    UGEN_FROM_ID(Strplay, strplay, id, "arco_strplay_ready");
    printf("arco_strplay_ready: id %d, ready %s, chans %d\n",
           id, ready ? "true" : "false", chans);
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


static void strplay_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/strplay/new", "isiffBBB", arco_strplay_new, NULL, true, true);
    o2sm_method_new("/arco/strplay/samps", "ih", arco_strplay_samps, NULL, true, true);
    o2sm_method_new("/arco/strplay/ready", "iiB", arco_strplay_ready, NULL, true, true);
    o2sm_method_new("/arco/strplay/play", "iB", arco_strplay_play, NULL, true, true);
    // END INTERFACE INITIALIZATION
}


Initializer strplay_init_obj(strplay_init);
