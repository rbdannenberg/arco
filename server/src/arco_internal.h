extern Terminal_ui *tu;  // import from arco.cpp
extern int midi_osc_line_count;  // import from arco.cpp
extern vector<string> arco_device_info;
extern char host_network_option[24];
extern bool host_o2lite_enable;
extern char host_ensemble_name[34];


int open_midi_osc_line();
char *heapify(const char *str);
char *arco_name_lookup(int id);
int lookup_device_id(string name);
void config_remove_midi_osc();
void create_midi_handlers();
