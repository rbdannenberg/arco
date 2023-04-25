// sugenid.cpp -- interface to [arcougenid.h]
//
// This file is machine generated, DO NOT EDIT

#include "sincl.h"

#include "o2.h"
#include "ugenid.h"
#ifndef _WIN32
namespace std {}; // in case std not in any header
using namespace std; // in case std is in a header
#endif
#include "arcougenid.h"

const char *ugen_id_name = "Ugen_id";
Ugen_id_descriptor ugen_id_descriptor;

void s2c_arco_ugen_new(Machine_ptr m)
{
    if (!m->error_flag) {
        Ugen_id * res = arco_ugen_new(m);
        SExtern_ptr p = SExtern::create(&ugen_id_descriptor, res, m);
        m->push(p);
    }
}


void s2c_arco_ugen_new_id(Machine_ptr m)
{
    SVal local0 = LOCAL_TO_SVAL(m->frame->local(0));
    int arg0 = (int) Node::must_get_long(local0, m);
    if (!m->error_flag) {
        Ugen_id * res = arco_ugen_new_id((int) arg0, m);
        SExtern_ptr p = SExtern::create(&ugen_id_descriptor, res, m);
        m->push(p);
    }
}


void s2c_arco_ugen_id(Machine_ptr m)
{
    SExtern_ptr arg0 = Node::must_get_extern(LOCAL_TO_SVAL(m->frame->local(0)), ugen_id_name, m);
    if (!m->error_flag) {
        int res = arco_ugen_id((Ugen_id *) (arg0->objptr));
        m->push(Node::create_long(res, m));
    }
}


void s2c_arco_ugen_epoch(Machine_ptr m)
{
    SExtern_ptr arg0 = Node::must_get_extern(LOCAL_TO_SVAL(m->frame->local(0)), ugen_id_name, m);
    if (!m->error_flag) {
        int res = arco_ugen_epoch((Ugen_id *) (arg0->objptr));
        m->push(Node::create_long(res, m));
    }
}


void s2c_arco_ugen_free(Machine_ptr m)
{
    SExtern_ptr arg0 = Node::must_get_extern(LOCAL_TO_SVAL(m->frame->local(0)), ugen_id_name, m);
    if (!m->error_flag) {
        arco_ugen_free((Ugen_id *) (arg0->objptr));
        m->push(NULL);
    }
}


void s2c_arco_ugen_gc(Machine_ptr m)
{
    if (!m->error_flag) {
        arco_ugen_gc(m);
        m->push(NULL);
    }
}


void s2c_arco_ugen_reset(Machine_ptr m)
{
    if (!m->error_flag) {
        int res = arco_ugen_reset();
        m->push(Node::create_long(res, m));
    }
}


void s2c_arco_ugen_gc_info(Machine_ptr m)
{
    if (!m->error_flag) {
        arco_ugen_gc_info(m);
        m->push(NULL);
    }
}


void sugenid_init_fn(Machine_ptr m)
{
    ugen_id_descriptor.name = ugen_id_name;
    m->create_builtin(Symbol::create("arco_ugen_new", m), 0, &s2c_arco_ugen_new);
    m->create_builtin(Symbol::create("arco_ugen_new_id", m), 1, &s2c_arco_ugen_new_id);
    m->create_builtin(Symbol::create("arco_ugen_id", m), 1, &s2c_arco_ugen_id);
    m->create_builtin(Symbol::create("arco_ugen_epoch", m), 1, &s2c_arco_ugen_epoch);
    m->create_builtin(Symbol::create("arco_ugen_free", m), 1, &s2c_arco_ugen_free);
    m->create_builtin(Symbol::create("arco_ugen_gc", m), 0, &s2c_arco_ugen_gc);
    m->create_builtin(Symbol::create("arco_ugen_reset", m), 0, &s2c_arco_ugen_reset);
    m->create_builtin(Symbol::create("arco_ugen_gc_info", m), 0, &s2c_arco_ugen_gc_info);
    Symbol_ptr s;
    s = Symbol::create("UGEN_TABLE_SIZE", m);
    s->set_value(Node::create_long(1000, m), m);
}


Builtin_init sugenid_init(sugenid_init_fn, "sugenid");
