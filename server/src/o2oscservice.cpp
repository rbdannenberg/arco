// o2oscservice.cpp -- support option for OSC I/O from o2host
//
// Roger B. Dannenberg
// Feb 2024

#include "o2internal.h"
#include "assert.h"
#include <string>
#include <vector>
#include <utility>
using std::string;
using std::vector;
using std::pair;
#include "fieldentry.h"

// Undefine Windows MOUSE_MOVED before including curses to avoid conflict
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif

#ifdef __linux__
#include "ncurses.h"
#else
#include "curses.h"
#endif

#include "termui.h"
#include "arco_internal.h"
#include "o2oscservice.h"

vector<string> tcp_udp_options = {"UDP", "TCP"};

// active open OSC connections for forwarding to/from O2:
vector<Osc_io_info> from_osc_input;
vector<Osc_io_info> to_osc_output;


void insert_o2_to_osc_fields(int y, string service, string ip,
                             string port, string tcp)
{
    // make "Service:" field
    int i = tu->field_string("Service:", "osc_out_srv", service,
                             0, y, 8, 20);
    Field_entry *oof = tu->fields[i];  // grab the field before i changes
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
    tu->set_current_field(oof);  // "Osc Out From" => "oof"

    // make "to OSC IP:" field
    i = tu->field_ip("to OSC IP:", "osc_out_ip", ip, 30, y, 10, 15);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make ":" (port) field
    i = tu->field_int(":", "osc_out_port", port, 0, 65535, 57, y, 1, 5);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make "TCP/UDP" field
    i = tu->field_menu("", "osc_out_tcp", tcp, 65, y, 0, 4, tcp_udp_options);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make "Del:" field
    i = tu->field_button("Del:", "osc_out_del", 72, y, 4, 1);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
}


int insert_o2_to_osc() {
    int y = open_midi_osc_line();
    insert_o2_to_osc_fields(y, "", "", "", "UDP");

    tu->dialog_refresh();
    return y;
}


void insert_osc_to_o2_fields(int y, string port, string tcp, string service)
{
    // make "Forward OSC Port:" field
    int i = tu->field_int("Forward OSC Port:", "osc_in_port", port,
                          0, 65535, 0, y, 17, 5);
    Field_entry *fop = tu->fields[i];  // grab the field before i changes
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
    tu->set_current_field(fop);  // "Osc Out From" => "oof"

    // make "TCP/UDP" field
    i = tu->field_menu("Via:", "osc_in_tcp", tcp, 23, y, 4, 4,
                       tcp_udp_options);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make "to Service:" field
    i = tu->field_string("to Service:", "osc_in_srv", service, 39, y, 11, 20);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make "Del:" field
    i = tu->field_button("Del:", "osc_in_del", 72, y, 4, 1);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
}


// make three fields and insert before key "arco_in_id"
// find "arco_in_id" -- that's the line we want to use
int insert_osc_to_o2()
{
    int y = open_midi_osc_line();
    insert_osc_to_o2_fields(y, "", "UDP", "");

    tu->dialog_refresh();
    return y;
}


void osc_input_initialize(const char *service_name, int port_num, bool tcp_flag)
{
    o2_osc_port_new(service_name, port_num, tcp_flag);
    Osc_io_info in_info{service_name, "", port_num, tcp_flag};
    from_osc_input.push_back(in_info);
}


void osc_output_initialize(const char *service, const char *ip, int port_num,
                           bool tcp_flag)
{
    o2_osc_delegate(service, ip, port_num, tcp_flag);
    Osc_io_info out_info{service, ip, port_num, tcp_flag};
    to_osc_output.push_back(out_info);
}


// decommission all osc input/output
void osc_input_output_finish()
{
    for (Osc_io_info &in_info : from_osc_input) {
        o2_osc_port_free(in_info.port);
        o2_service_free(in_info.service.c_str());
    }
    from_osc_input.clear();

    for (Osc_io_info &out_info : to_osc_output) {
        o2_service_free(out_info.service.c_str());
    }
    to_osc_output.clear();
}
