// midiservice.h -- support option for MIDI I/O
//
// Roger B. Dannenberg
// Feb 2024

class Midi_io_info {
  public:
    string device_name;
    string address_or_service;
    PmStream *stream;

  Midi_io_info(string device, string address_or_service_, PmStream *pmstream) {
      device_name = device;
      address_or_service = address_or_service_;
      stream = pmstream;
  }
};

extern vector<Midi_io_info> from_midi_input;
extern vector<Midi_io_info> to_midi_output;


void insert_midi_to_o2_fields(int y, string init_selection, string service);

void insert_o2_to_midi_fields(int y, string service, string device);

void get_midi_device_options();

void free_midi_device_names();

// reconstruct menus
void midi_devices_refresh();

void midi_input_initialize(const char *device, string &service);

void midi_output_initialize(string &service, const char *device);

void midi_poll();

void insert_midi_to_o2();

void insert_o2_to_midi();

void midi_services_finish();

