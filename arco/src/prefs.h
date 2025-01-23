/* prefs - preference read/write
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

char *prefs_in_name();
char *prefs_out_name();
int prefs_in_lookup(const char *name, int id);
int prefs_out_lookup(const char *name, int id);
int prefs_in_chans();
int prefs_out_chans();
int prefs_buffer_size();
int prefs_latency_ms();

void prefs_set_latency(int latency);
void prefs_set_in_name(const char *name);
void prefs_set_out_name(const char *name);
void prefs_set_in_chans(int chans);
void prefs_set_out_chans(int chans);
void prefs_set_buffer_size(int size);

extern char p_in_name[80];
extern char p_out_name[80];
extern int p_in_id;
extern int p_out_id;
extern int p_in_chans;
extern int p_out_chans;
extern int p_buffer_size;
extern int p_latency_ms;
