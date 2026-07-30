// midiservice.cpp -- support option for MIDI I/O from server
//
// Roger B. Dannenberg
// Feb 2024

#include "o2internal.h"
#include "assert.h"
#include <string>
#include <vector>
using std::vector;
using std::string;
#include "fieldentry.h"
#include "curses.h"
#include "termui.h"
#include "portmidi.h"
#include "arco_internal.h"
#include "midiservice.h"


// these are non-null after get_midi_device_options is called
// these would naturally be char *[] with an array that points to
// the strings allocated within PortMidi, but we want to use them
// as options in the configuration dialog, and options have to be
// vector<string>. So we copy strings from PortMidi to these options.
vector<string> midi_in_devices;
vector<string> midi_out_devices;
bool midi_device_info_valid = false;

vector<Midi_io_info> from_midi_input;
vector<Midi_io_info> to_midi_output;


void get_midi_device_options()
{
    if (midi_device_info_valid) {
        return;
    }
    Pm_Initialize();
    // need to construct options
    int n = Pm_CountDevices();

    for (int i = 0; i < n; i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info->input) {
            midi_in_devices.push_back(info->name);
        } else {
            midi_out_devices.push_back(info->name);
        }
    }
    midi_device_info_valid = true;
    // now we have option lists for all midi devices
}


void free_midi_device_names()
{
    // note that device names are owned by PortMidi - do not free them!
    midi_in_devices.clear();
    midi_out_devices.clear();
    midi_device_info_valid = false;
}


// reconstruct menus
void midi_devices_refresh()
{
    if (midi_in_devices.size() > 0 || midi_out_devices.size() > 0) {
        free_midi_device_names();
        Pm_Terminate();
    }
    get_midi_device_options();  // does Pm_Initialize()

    // restore all fields with valid midi device names
    for (Field_entry *field : tu->fields) {
        if (field->key == "midi_out_dev") {
            field->options = &midi_out_devices;
            field->show_content(tu);
        } else if (field->key == "midi_in_dev") {
            field->options = &midi_in_devices;
            field->show_content(tu);
        }
    }
    create_midi_handlers();

}

// get midi device ID for name. input is 1 for input, 0 for output
int midi_name_to_id(const char *name, int input)
{
    for (int i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if ((info->input != 0) == (input != 0) &&
            strcmp(info->name, name) == 0) {
            return i;
        }
    }
    return -1;
}


void insert_o2_to_midi_fields(int y, string service, string device)
{
    // make "MIDI Out Service:" field
    int i = tu->field_string("MIDI Out Service:", "midi_out_srv", service,
                             0, y, 17, 20);
    Field_entry *mof = tu->fields[i];  // grab the field before i changes
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
    tu->set_current_field(mof);

    // make "to:" field
    i = tu->field_menu("to:", "midi_out_dev", device,
                       39, y, 3, 20, midi_out_devices);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make "Del:" field
    i = tu->field_button("Del:", "midi_out_del", 72, y, 4, 1);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
}


// make three fields and insert before key "arco_in_id"
// find "arco_in_id" -- that's the line we want to use
int insert_o2_to_midi() {
    get_midi_device_options();
    int y = open_midi_osc_line();

    // make Midi Out initial content: 
    const char *init_selection = midi_out_devices.size() > 0 ?
            midi_out_devices[0].c_str() : "NO MIDI-OUT DEVICE!";
    insert_o2_to_midi_fields(y, "", init_selection);
    tu->dialog_refresh();
    // redraw_requested = true;
    return y;
}


void insert_midi_to_o2_fields(int y, string device, string service)
{
    int i = tu->field_menu("MIDI In:", "midi_in_dev", device,
                           0, y, 8, 28, midi_in_devices);
    Field_entry *mif = tu->fields[i];  // grab the field before i changes
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
    tu->set_current_field(mif);

    // make "to Service:" field
    i = tu->field_string("to Service:", "midi_in_srv", "", 39, y, 11, 20);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id

    // make "Del:" field
    i = tu->field_button("Del:", "midi_in_del", 72, y, 4, 1);
    tu->dialog_move_field(i, "arco_in_id", true);  // move before in_id
}


// Insert forms into dialog for configuring midi to O2 service.
// If there are no midi input devices, display "NO MIDI IN DEVICE!"
int insert_midi_to_o2() {
    get_midi_device_options();
    int y = open_midi_osc_line();

    // make Midi In field:
    const char *init_selection = midi_in_devices.size() > 0 ?
            midi_in_devices[0].c_str() : "NO MIDI-IN DEVICE!";
    insert_midi_to_o2_fields(y, init_selection, "");

    tu->dialog_refresh();
    // redraw_requested = true;
    return y;
}


void print_pmerror(PmError pm_err)
{
    const char *msg;
    if (pm_err != pmHostError) {
        msg = Pm_GetErrorText(pm_err);
    } else {
        char message[80];
        Pm_GetHostErrorText(message, 80);
        msg = message;
    }
    printf("%s\n", msg);
}


void midi_input_initialize(const char *device, string &service)
{
    printf("MIDI input %s to O2 address /%s/midi\n", device, service.c_str());
    int dev_id = midi_name_to_id(device, 1);
    if (dev_id < 0) {
        printf("WARNING: MIDI input %s is not (no longer) available\n",
               device);
    } else {
        PmStream *midi_in;
        PmError pmerr = Pm_OpenInput(&midi_in, dev_id, NULL, 100, NULL, NULL);
        if (pmerr) {
            printf("WARNING: Could not open %s because ", device);
            print_pmerror(pmerr);
        } else {
            Midi_io_info in_info{device, "/" + service + "/midi", midi_in};
            from_midi_input.push_back(in_info);
        }
    }
}


void midi_message_handler(O2_HANDLER_ARGS)
{
    intptr_t output_index = (intptr_t) user_data;
    if ((types[0] == 'i' || types[0] == 'm') && types[1] == 0) {
        int midi_msg = *((int32_t *)o2_msg_data_params(types));
        printf("got midi message from o2, address %s: %x\n",
               msg->address, midi_msg);
        Pm_WriteShort(to_midi_output[output_index].stream, 0, midi_msg);
    }
}


void midi_output_initialize(string &service, const char *device)
{
    printf("MIDI Output from O2 address /%s/midi to %s\n",
           service.c_str(), device);
    int dev_id = midi_name_to_id(device, 0);
    PmStream *midi_out = NULL;
    if (dev_id < 0) {
        printf("WARNING: MIDI output %s is not (no longer) available\n",
               device);
    } else {
        PmError pmerr = Pm_OpenOutput(&midi_out, dev_id, NULL, 100, 
                                      NULL, NULL, 0);
        if (pmerr) {
            printf("WARNING: Could not open %s: ", device);
            return;
            /*
            char msg[128];
            snprintf(msg, 128, "WARNING: Could not open %s: ", device);
            printf("%s", msg);
            print_pmerror(pmerr);
            print_error(msg);
            return;
            */
        }
    }
    Midi_io_info out_info{device, service, midi_out};
    to_midi_output.push_back(out_info);
    o2_service_new(service.c_str());
    string address = "/" + service + "/midi";
    O2err o2err = o2_method_new(address.c_str(), NULL, midi_message_handler,
                       (void *) (intptr_t) (to_midi_output.size() - 1),
                       false, false);
    if (o2err) {
        printf("Error: could not create handler for %s\n", address.c_str());
        Pm_Close(midi_out);
        to_midi_output.pop_back();
        /*
        char msg[128];
        snprintf(msg, 128, "Error: could not create handler for %s\n", address);
        print_error(msg);
        */
        return;
    }
}


void midi_services_finish()
{
    // free MIDI outputs
    for (Midi_io_info &out_info : to_midi_output) {
        o2_service_free(out_info.address_or_service.c_str());
        Pm_Close(out_info.stream);
    }
    to_midi_output.clear();

    // free MIDI inputs
    for (Midi_io_info &in_info : from_midi_input) {
        Pm_Close(in_info.stream);
    }
    from_midi_input.clear();
}


void midi_poll()
{
    PmEvent buffer[10];
    int i = 0;
    for (Midi_io_info &in_info : from_midi_input) {
        int n = Pm_Read(in_info.stream, buffer, 10);
        // ignore errors (n < 0)
        for (int j = 0; j < n; j++) {
            o2_send_cmd(in_info.address_or_service.c_str(), 0, "i",
                        buffer[j].message);
        }
        i++;
    }
}
