/* fileio.cpp -- asynchronous file io system for Arco
 *
 * Roger B. Dannenberg
 * April 2023
 *
 * See fileio.h for description.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "sys/timeb.h"
#include "pthread.h"
#include "sndfile.h"
#include "o2internal.h"  // need internal to offer bridge
#include "o2atomic.h"
#include "sharedmem.h"
#include "sharedmemclient.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "arcoutil.h"
#include "audioio.h"
#include "audioblock.h"
#include "ugen.h"
#include "fileio.h"

#define D if (0)

extern O2sm_info *audio_bridge;
    
O2sm_info *fileio_bridge = NULL;
Vec<Fileio_obj *> fileio_objs;
static pthread_t fileio_thread_id;
static bool fileio_thread_created = false;
static O2_context fio_o2_ctx;
static bool fileio_quit_request = false;


class Fileio_reader : public Fileio_obj {
public:
    float start;
    float end;
    bool cycle;
    bool file_is_open;
    
    SNDFILE *snd_in;  // file descriptor
    SF_INFO snd_in_info;  // sfinfo structure (sample rate, format, etc.)
    Audioblock *blocks[2];  // pointers to buffers
    int64_t all_frames_count;  // how many frames to read from file
    int64_t frames_to_end;  // how many more frames to read until end time
    int block_on_deck;  // which block do we send next?
    

    Fileio_reader(int64_t addr, char *fn, float st, float en, bool cy) :
            Fileio_obj(addr) {
        int rslt = -1;  // failure until we open file and seek
        start = st;
        end = en;
        cycle = cy;
        all_frames_count = 2000000000000;  // we'll stop at eof
        
        file_is_open = false;
        block_on_deck = 0;
        snd_in_info.format = 0;
        snd_in = sf_open(fn, SFM_READ, &snd_in_info);
        if (snd_in) {
            file_is_open = true;
            rslt = 0;
        }
        if (!file_is_open) {
            arco_print("Fileio_reader: Failed to open %s\n", fn);
            snd_in_info.channels = 0;  // make sure we indicate an error
            cycle = false;  // pretend we're at the end of the file and no cycle
            all_frames_count = 0;
        } else if (end > 0) {
            all_frames_count = (end - start) * snd_in_info.samplerate;
        }

        int chans  = snd_in_info.channels;
        blocks[0] = audioblock_alloc(chans);
        blocks[1] = audioblock_alloc(chans);

        if (start > 0.0 && file_is_open) {
            rslt = (int) sf_seek(snd_in, (sf_count_t)
                                 (start * snd_in_info.samplerate), SEEK_SET);
            if (rslt < 0) {  // error in seek -- give up and close the file
                cycle = false;  // pretend we're at the end of file and no cycles
                all_frames_count = 0;
            }
        }
        frames_to_end = all_frames_count;
        
        // o2sm_send_cmd("/arco/fileplay/ready", 0, "hiB", addr,
        //               (file_is_open ? chans : 0), rslt < 0);
        o2_send_start();
        o2_add_int64(addr);
        o2_add_int32(file_is_open ? chans : 0);
        o2_add_bool(rslt >= 0);
        O2message_ptr msg = o2_message_finish(0.0, "/arco/fileplay/ready", true);
        audio_bridge->outgoing.push((O2list_elem *) msg);

        load_block();  // even if error opening or seeking

        arco_print("Opened %s, a %d channel audio file.\n", fn, chans);
    }


    ~Fileio_reader() {
        if (blocks[0]) {
            O2_FREE(blocks[0]);
            O2_FREE(blocks[1]);
        }
        makeclosed();
    }


    void load_block() {
        Audioblock *ablock = blocks[block_on_deck];

        // read if we need to
        int frames_to_go = AUDIOBLOCK_FRAMES;

        while (all_frames_count > 0 && frames_to_go > 0) {
            // compute how many frames we can read. Read until we hit
            //     either AUDIOBLOCK_FRAMES or all_frames_count:
            int64_t frames = MIN(frames_to_end, frames_to_go);
            if (frames > 0) {
                int frames_read = (int) sf_readf_short(snd_in, ablock->dat,
                                                       frames);
                if (frames_read < frames) {
                    // we hit end of samples or a read error: act as if EOF
                    all_frames_count = 0;
                }
                frames_to_end -= frames_read;
                frames_to_go -= frames_read;
            }
            if (frames_to_end == 0 && cycle) {  // reopen and start reading again
                sf_seek(snd_in, (sf_count_t) (start * snd_in_info.samplerate),
                        SEEK_SET);
                frames_to_end = all_frames_count;
            }
        }
        
        ablock->frames = AUDIOBLOCK_FRAMES - frames_to_go;
        ablock->channels = snd_in_info.channels;
        // if we're out of frames, then tell player we are done:
        ablock->last = frames_to_go > 0;

        // o2sm_send_cmd("/arco/fileplay/samps", 0, "hh", addr, (int64_t) ablock);
        // instead of sending through O2, deliver straight to Arco:
        o2_send_start();
        o2_add_int64(addr);
        o2_add_int64((int64_t) ablock);
        O2message_ptr msg = o2_message_finish(0.0, "/arco/fileplay/samps", true);
        audio_bridge->outgoing.push((O2list_elem *) msg);

        block_on_deck ^= 1;  // swap 0 <-> 1
        if (ablock->last) {
            makeclosed();
        }
    }
    
    
    void makeclosed() {
        if (file_is_open) {
            sf_close(snd_in);
            file_is_open = false;
        }
    }
};


class Fileio_writer : public Fileio_obj {
public:
    bool file_is_open;
    
    SNDFILE *snd_out;  // file descriptor
    SF_INFO snd_out_info;  // sfinfo structure (sample rate, format, etc.)

    Fileio_writer(int64_t addr, int chans, char *fn) : Fileio_obj(addr) {
        int rslt = -1;  // failure until we open file and seek
        file_is_open = false;
        snd_out_info.frames = 0;
        snd_out_info.samplerate = AR;
        snd_out_info.channels = chans;
        snd_out_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        snd_out_info.sections = 0;
        snd_out_info.seekable = 0;
        snd_out = sf_open(fn, SFM_WRITE, &snd_out_info);
        if (snd_out) {
            file_is_open = true;
            rslt = 0;
        }
        if (!file_is_open) {
            arco_print("Fileio_writer: Failed to open %s\n", fn);
        }
        // o2sm_send_cmd("/arco/filerec/ready", 0, "hB", addr, rslt >= 0)
        o2_send_start();
        o2_add_int64(addr);
        o2_add_bool(rslt >= 0);
        O2message_ptr msg = o2_message_finish(0.0, "/arco/filerec/ready", true);
        audio_bridge->outgoing.push((O2list_elem *) msg);
    }


    ~Fileio_writer() {
        makeclosed();
    }


    bool write_block(int64_t block_addr) {  // return true if last block
        Audioblock *block = (Audioblock *) block_addr;
        D printf("fileio::write_block dat address %p\n", block->dat);
        sf_writef_short(snd_out, block->dat, block->frames);
        D printf("fileio::write_block wrote %d frames\n", block->frames);
        
        /* DEBUG: write sample values as text to testout.txt:
        static FILE *outf = NULL;
        if (!outf) outf = fopen("testout.txt", "w");
        for (int i = 0; i < block->frames; i++) {
            fprintf(outf, "%d\n", block->dat[i]);
        }
        if (block->frames < 8000) {
            fclose(outf);
        }
        */
        
        /* DEBUG: Look for 4 zeros in a row -- this was some test looking for
         *    blocks that were missing data, but in practice, 4 zeros in a row
         *    is not too uncommon, even from live microphone input
        for (int i = 0; i < block->frames - 4; i++) {
            int j;
            for (j = 0; j < 4; j++) {
                if (block->dat[i + j] != 0) break;
            }
            // if j==4, we found 4 zeros;
            // must also be preceded and/or followed by non-zero
            if (j == 4) {
                if ((i == 0 || block->dat[i - 1] != 0) &&
                    (i + j < block->frames && block->dat[i + j] != 0)) {
                    printf("WE FOUND BAD DATA!!!!! at %d\n", i);
                }
            }
        }
        */

        // o2sm_send_cmd("/arco/fileplay/samps", 0, "hh", addr);
        // instead of sending through O2, deliver straight to Arco:
        o2_send_start();
        o2_add_int64(addr);
        O2message_ptr msg = o2_message_finish(0.0, "/arco/filerec/samps", true);
        audio_bridge->outgoing.push((O2list_elem *) msg);
        return block->last;
    }
    
    
    void makeclosed() {
        if (file_is_open) {
            sf_close(snd_out);
            file_is_open = false;
        }
    }
};


static int fileio_find(int64_t addr)
{
    for (int i = 0; i < fileio_objs.size(); i++) {
        Fileio_obj *fobj = fileio_objs[i];
        if (fobj->addr == addr) {
            return i;
        }
    }
    return -1;
}


/* O2SM INTERFACE: /fileio/fileplay/new
       int64 addr,
       string filename,
       float start, float end,
       bool cycle;
   Create a new file reader and open the file.
*/
static void fileio_fileplay_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    char *filename = argv[1]->s;
    float start = argv[2]->f;
    float end = argv[3]->f;
    bool cycle = argv[4]->B;
    // end unpack message

    fileio_objs.push_back(new Fileio_reader(addr, filename, start, end, cycle));
}


/* O2SM INTERFACE: /fileio/fileplay/read int64 addr; */
void fileio_fileplay_read(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    // end unpack message

    int i = fileio_find(addr);
    if (i >= 0) {
        ((Fileio_reader *) fileio_objs[i])->load_block();
    }
}           


/* O2SM INTERFACE: /fileio/fileplay/play int64 addr, bool play_flag; */
void fileio_fileplay_play(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    bool play_flag = argv[1]->B;
    // end unpack message

    int i = fileio_find(addr);
    if (i >= 0) {
        Fileio_reader *reader = (Fileio_reader *) fileio_objs[i];
        if (play_flag) {
            reader->load_block();
        } else {
            fileio_objs.remove(i);
            printf("fileio_fileplay_play deleting reader @ %p addr %p\n",
                   reader, (void *) addr);
            delete reader;
        }
    }
}           


/* O2SM INTERFACE: /fileio/quit; */
void fileio_quit(O2SM_HANDLER_ARGS)
{
    fileio_quit_request = true;
}


// entry point for fileio thread -- it runs o2poll
void *fileio_thread_run(void *data)
{
    o2_ctx = &fio_o2_ctx;
    while (!fileio_quit_request) {
        o2sm_poll();
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000;
        select(0, NULL, NULL, NULL, &timeout);
    }
    // time to quit
    while (fileio_objs.size() > 0) {
        delete fileio_objs[0];
    }
    fileio_objs.finish();
    o2sm_finish();
    return NULL;
}


/* O2SM INTERFACE: /fileio/filerec/new
       int64 addr,
       int32 chans,
       string filename;
   Create a new file reader and open the file.
*/
static void fileio_filerec_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int32_t chans = argv[1]->i;
    char *filename = argv[2]->s;
    // end unpack message

    fileio_objs.push_back(new Fileio_writer(addr, chans, filename));
}


/* O2SM INTERFACE: /fileio/filerec/write int64 addr, int64 block; */
void fileio_filerec_write(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int64_t addr = argv[0]->h;
    int64_t block = argv[1]->h;
    // end unpack message
    int i = fileio_find(addr);
    D printf("fileio_filerec_write addr %p block %p index %d last %d\n",
           (void *) addr, (void *) block, i, ((Audioblock *) block)->last);
    if (i >= 0) {
        Fileio_writer *writer = (Fileio_writer *) fileio_objs[i];
        bool last = writer->write_block(block);
        D printf("wrote audioblock %p, last %d\n", (void *) block, last);
        o2_send_start();
        o2_add_int64(addr);
        O2message_ptr msg = 
                o2_message_finish(0.0, "/arco/filerec/samps", true);
        audio_bridge->outgoing.push((O2list_elem *) msg);
        if (last) {
            fileio_objs.remove(i);
            printf("fileio_filerec_write deleting writer @ %p addr %p\n",
                   writer, (void *) addr);
            delete writer;
        }
    }
}           


// to be called from main thread - creates and starts fileio thread
// returns non-zero error code if pthread_create fails
//
int fileio_initialize()
{
    // create the bridge and instance
    assert(fileio_bridge == NULL);
    fileio_bridge = o2_shmem_inst_new();
    O2_context *save = o2_ctx;
    o2sm_initialize(&fio_o2_ctx, fileio_bridge);
    o2sm_service_new("fileio", NULL);

    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/fileio/fileplay/new", "hsffB", fileio_fileplay_new, NULL, true, true);
    o2sm_method_new("/fileio/fileplay/read", "h", fileio_fileplay_read, NULL, true, true);
    o2sm_method_new("/fileio/fileplay/play", "hB", fileio_fileplay_play, NULL, true, true);
    o2sm_method_new("/fileio/quit", "", fileio_quit, NULL, true, true);
    o2sm_method_new("/fileio/filerec/new", "his", fileio_filerec_new, NULL, true, true);
    o2sm_method_new("/fileio/filerec/write", "hh", fileio_filerec_write, NULL, true, true);
    // END INTERFACE INITIALIZATION

    // create a thread to poll for fileio
    int rslt = pthread_create(&fileio_thread_id, NULL, fileio_thread_run, NULL);
    if (rslt != 0) return rslt;
    fileio_thread_created = true;

    o2_ctx = save;  // restore caller (probably main O2 thread)'s context
    return 0;
}
