// sarco.cpp -- interface to [audioio.h, arco3_init.h]
//
// This file is machine generated, DO NOT EDIT

#include "sincl.h"

#include "arcotypes.h"
#include "o2internal.h"
#ifndef WIN32
namespace std {}; // in case std not in any header
using namespace std; // in case std is in a header
#endif
#include "audioio.h"

void s2c_arco_thread_poll(Machine_ptr m)
{
    if (!m->error_flag) {
        arco_thread_poll();
        m->push(NULL);
    }
}


#include "arco4_init.h"

void s2c_arco3_initialize(Machine_ptr m)
{
    if (!m->error_flag) {
        arco3_initialize();
        m->push(NULL);
    }
}


void sarco_init_fn(Machine_ptr m)
{
    m->create_builtin(Symbol::create("arco_thread_poll", m), 0, &s2c_arco_thread_poll);
    m->create_builtin(Symbol::create("arco3_initialize", m), 0, &s2c_arco3_initialize);
}


Builtin_init sarco_init(sarco_init_fn, "sarco");
