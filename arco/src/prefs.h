/* prefs - preference read/write
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

int prefs_latency(int dflt);
int prefs_in_device(const char *name, int id, int *dflt);
int prefs_out_device(const char *name, int id, int *dflt);
int prefs_in_chans(int dflt);
int prefs_out_chans(int dflt);
int prefs_buffer_size(int dflt);

int prefs_set_latency(int latency);
int prefs_set_in_device(const char *name);
int prefs_set_out_device(const char *name);
int prefs_set_in_chans(int chans);
int prefs_set_out_chans(int chans);
int prefs_set_buffer_size(int size);

