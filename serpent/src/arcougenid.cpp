/* arcougenid.cpp -- Ugen id management for Serpent and GC
 *
 * Roger B. Dannenberg
 * April 2023
 */

#include "sincl.h"
#include "o2.h"
#include "ugenid.h"  // need UGEN_TABLE_SIZE
#include "arcougenid.h"

bool Ugen_id::initialized = false;
int Ugen_id::free_list = -1;
int Ugen_id::free_count = 0;
int Ugen_id::to_be_freed_list = -1;
int Ugen_id::to_be_freed_count = 0;
int32_t Ugen_id::current_arco_epoch = 0;
int Ugen_id::max_ugen_id_used = 0;
int32_t Ugen_id::ugen_id_table[UGEN_TABLE_SIZE];


int arco_ugen_id(Ugen_id_ptr ugen_id)
{
    int id = ugen_id->id;
    return ((ugen_id->epoch == Ugen_id::current_arco_epoch) && (id >= 0) &&
            (Ugen_id::ugen_id_table[id] == id || id < 10) ? id : -1);
}


int arco_ugen_epoch(Ugen_id_ptr ugen_id)
{
    return ugen_id->epoch;
}


void arco_ugen_free(Ugen_id_ptr ugen_id)
{
    ugen_id->free_now();
}


void arco_ugen_gc(Machine_ptr m)
{
    Ugen_id::free_to_be_freed(m);
}


int arco_ugen_reset()
{
    Ugen_id::initialized = false;
    return ++Ugen_id::current_arco_epoch;
}


void arco_ugen_gc_info(Machine_ptr m)
{
    printf("arco_ugen_gc_info: free ids %d, to be freed %d, epoch %d, "
           "gc cycles %lld\n", Ugen_id::free_count, Ugen_id::to_be_freed_count,
           Ugen_id::current_arco_epoch, m->gc_cycles);
}
