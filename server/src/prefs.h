/* prefs - preference read/write
 *
 * Roger B. Dannenberg
 * Feb 2022
 * Jul 2026 - moved to server/src, merged with svprefs.h
 */


extern ConfigManager configs;
extern Config *current_config;

#define DFLT_IN_NAME ""
#define DFLT_OUT_NAME ""
#define DFLT_IN_ID -1
#define DFLT_OUT_ID -1
#define DFLT_IN_CHANS -1
#define DFLT_OUT_CHANS -1
#define DFLT_BUFFER_SIZE -1
#define DFLT_LATENCY_MS -1
#define DFLT_NETWORK_OPTION ""
#define DFLT_O2LITE_ENABLE true
#define DFLT_HTTP_ENABLE false
#define DFLT_HTTP_PORT 8080
#define DFLT_HTTP_ROOT ""
#define DFLT_DEBUG_FLAGS ""
#define DFLT_POLLING_RATE 500
#define DFLT_REFERENCE_CLOCK false
#define DFLT_MQTT_PORT 8080
#define DFLT_MQTT_HOST ""
#define DFLT_ENSEMBLE_NAME "arco"


bool config_to_prefs();
void prefs_to_config();
void load_configurations(const char *default_configuration);
void prefs_write();

// These functions look up ids based on p_in_name and p_out_name,
// results may change and depend on what devices are available.
// If the preferred device name does not match any available
// devices, -1 is returned, which is PortAudio's "use default" value.
int prefs_in_id();
int prefs_out_id();


/*
 // When PortAudio provides a (new) list of devices, you can call
// this function to search and see if info contains prefs_in_name()
// or prefs_out_name(). If yes, then the in_id or out_id is set
// according to the device number at the beginning of info. Before
// iterating through PortAudio device info, you should call
// prefs_set_in_id(-1) and prefs_set_out_id(-1) to mark the ids
// as "default" in case name preferences do not match any actual
// device that's currently available.
void prefs_id_lookup(const char *info);
*/

char *prefs_in_name();
char *prefs_out_name();
int prefs_in_chans();
int prefs_out_chans();
int prefs_buffer_size();
int prefs_latency_ms();
const char *prefs_network_option();
bool prefs_o2lite_enable();
bool prefs_http_enable();
int prefs_http_port();
const char *prefs_http_root();
const char *prefs_debug_flags();
const char *prefs_ensemble_name();
int prefs_polling_rate();
bool prefs_reference_clock();
const char *prefs_mqtt_host();
int prefs_mqtt_port();

void prefs_set_latency(int latency);
void prefs_set_in_id(int id);  // actually sets name after lookup
void prefs_set_out_id(int id);  // actually sets name after lookup
void prefs_set_in_name(const char *name);
void prefs_set_out_name(const char *name);
void prefs_set_in_chans(int chans);
void prefs_set_out_chans(int chans);
void prefs_set_buffer_size(int size);
void prefs_set_network_option(const char *option);
void prefs_set_o2lite_enable(bool enable);
void prefs_set_http_enable(bool enable);
void prefs_set_http_port(int port);
void prefs_set_http_root(const char *root);
void prefs_set_debug_flags(const char *flags);
void prefs_set_ensemble_name(const char *flags);
void prefs_set_polling_rate(int rate);
void prefs_set_reference_clock(bool enable);
void prefs_set_mqtt_host(const char *host);
void prefs_set_mqtt_port(int port);


/* these are derived from names outside of prefs.cpp. Maybe they
 * should not even be declared here, but I'm leaving them for now.
 */
extern int p_in_id;
extern int p_out_id;
extern int p_in_chans;
extern int p_out_chans;
