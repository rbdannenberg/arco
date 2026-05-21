/* arcougenid.cpp -- Ugen id management for Serpent and GC
 *
 * Roger B. Dannenberg
 * April 2023
 */

#include "sincl.h"
#include "o2.h"
#include "ugenid.h"  // need UGEN_TABLE_SIZE
#include "arcougenid.h"

static bool initialized = false;
static int free_list = UGEN_END_OF_LIST;
static int free_count = 0;
static int to_be_freed_list = UGEN_END_OF_LIST;
static int to_be_freed_count = 0;
static int32_t current_arco_epoch = 0;
static int max_ugen_id_used = 0;
static int32_t ugen_id_table[UGEN_TABLE_SIZE];

static void initialize() {
    for (int i = 10; i < UGEN_TABLE_SIZE; i++) {
        ugen_id_table[i] = UGEN_LINK(i + 1);
    }
    free_list = 10; // all id's from 10 are initially free
    to_be_freed_list = UGEN_END_OF_LIST; // empty to-be-freed list
    to_be_freed_count = 0;
    for (int i = 2; i < 10; i++) {
        ugen_id_table[i] = UGEN_LINK(UGEN_END_OF_LIST);  // unavailable slots
    }
    ugen_id_table[UGEN_TABLE_SIZE - 1] = UGEN_LINK(UGEN_END_OF_LIST);
    free_count = UGEN_TABLE_SIZE - 10;
    initialized = true;
    printf("**** Ugen_id table (re)initialized ****\n");
}


// make_ugen_id - construct representation of Ugen_id to store in 
//     Serpent's extern objptr field
int64_t make_ugen_id()
{
    // find the array of ids
    if (!initialized) {
        initialize();
    }
    // head pointer is free_list
    int32_t id = free_list;
    if (id != UGEN_END_OF_LIST) {  // list has an id
        free_list = UGEN_IDX2ID(id);
        // printf("*** make_ugen_id: popped %d off free_list, which is now %d\n",
        //       id, free_list);
        assert(UGEN_IN_RANGE(free_list));
        ugen_id_table[id] = UGEN_LINK(id);  // indicate "allocated"
        if (id > max_ugen_id_used) {
            max_ugen_id_used = id;
            // printf("********************************** max id %d\n", id);
        }
        free_count--;
    }
    return (((int64_t) current_arco_epoch) << 32) | id;
    // printf("Allocated ugen %d(%d) free_list %d\n", id, epoch, free_list);
}


// make_ugen_id_from - construct representation of Ugen_id from an id
//     to store in Serpent's extern objptr field
int64_t make_ugen_id_from(int id)
{
    assert(id < 10);
    if (!initialized) {
        initialize();
    }
    return (((int64_t) current_arco_epoch) << 32) | id;
}


// arco_ugen_id - extract the id if it is valid, o.w. -1
//
int arco_ugen_id(Ugen_id_ptr ugen_id)
{
    int id = UGEN_ID(ugen_id);
    int epoch = UGEN_EPOCH(ugen_id);
    return ((epoch == current_arco_epoch) && (id >= 0) &&
            (id < 10 || UGEN_IDX2ID(id) == id) ? id : -1);
}

// arco_id_is_valid - test if id is a valid id that references an
//     instance of a Ugen (assuming current epoch)
bool arco_id_is_valid(int id)
{
    return (id > 0) && (id < UGEN_TABLE_SIZE) && (UGEN_IDX2ID(id) == id);
}

int arco_ugen_epoch(Ugen_id_ptr ugen_id)
{
    return UGEN_EPOCH(ugen_id);
}


static void send_free_msg(int32_t id) {
    o2_send_cmd("/arco/free", 0, "i", id);
}


// arco_ugen_free - explicit free ugen_id without waiting
//    for GC
void arco_ugen_free(int id)
{
    if (arco_id_is_valid(id)) {
        send_free_msg(id);
        // now mark as freed before GC
        ugen_id_table[id] = UGEN_FREE_SENT_NO_GC(id);
        // printf("*** arco_ugen_free: marking %d as freed before GC\n", id);
    } else {
        printf("WARNING: arco_ugen_free called with invalid id %d\n", id);
    }
}


void arco_ugen_gc(Machine_ptr mach)
{
    if (!initialized) {
        return;
    }
    // give a boost to GC processing when 1/2 of id's are allocated
    // we run gc_poll() UGEN_TABLE_SIZE/(2n) times when there are n
    // free id's, so the boost goes up in proportion to 1/n.
    if (to_be_freed_list == UGEN_END_OF_LIST) {
        int n = free_count;
        while (n < UGEN_TABLE_SIZE / 2) {
            mach->gc_poll();
            printf("gc boost %d/%d ids are free\n",
                    free_count, UGEN_TABLE_SIZE);
            n += free_count + 1;  // make sure loop terminates!
        }
    }
    while (to_be_freed_list != UGEN_END_OF_LIST) {
        int id = to_be_freed_list;
        send_free_msg(id);
        // printf("*** arco_ugen_gc: moving %d to the free list\n", id);
        // move from to-be-freed to FREE_SENT state:
        to_be_freed_list = UGEN_IDX2ID(id); // pop
        ugen_id_table[id] = UGEN_LINK(free_list);
        free_list = id;
        free_count++;
    }
    to_be_freed_count = 0;
}



int arco_ugen_reset()
{
    initialized = false;
    return ++current_arco_epoch;
}


void arco_ugen_gc_info(Machine_ptr m)
{
    printf("arco_ugen_gc_info: free ids %d, to be freed %d, epoch %d, "
           "gc cycles %lld\n", free_count, to_be_freed_count,
           current_arco_epoch, m->gc_cycles);
}


SVal Ugen_id_descriptor::repr(SExtern_ptr obj, Machine_ptr m)
{
    char msg[128];
    Ugen_id_ptr ugen = (Ugen_id_ptr) (obj->objptr);
    sprintf(msg, "<ArcoUgen %d(%d)>", UGEN_ID(ugen), UGEN_EPOCH(ugen));
    return SString::create(msg, SS_NFC, m);
}


long Ugen_id_descriptor::free(SExtern_ptr obj, Machine_ptr m)
 {
     Ugen_id_ptr ugen_id = (Ugen_id_ptr) (obj->objptr);
     int id = arco_ugen_id(ugen_id);
     if (id > 0) {  // add to the to-be-freed list
         int state = UGEN_IDX2STATE(id);
         // printf("*** Ugen_id_descriptor::free: id %d state %d\n", id, state);
         if ((state == ON_LIST && UGEN_IDX2ID(id) == id) ||
             (state == FREE_SENT_NO_GC)) {
             ugen_id_table[id] = UGEN_LINK(to_be_freed_list);
             to_be_freed_list = id;
             to_be_freed_count++;
             // printf("GC: Ugen %d(%d) added to to-be-freed list\n",
             //        id, current_arco_epoch);
         }
     }   
     return 4;
 }
