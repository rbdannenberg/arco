/* prefs - preference interface
 *
 * Roger B. Dannenberg
 * Feb 2022
 * Jul 2026 - moved to server/src, merged with svprefs.cpp
 */

/* This module is an exchange between code that manages preferences
   and any Arco code that needs to know preferences.
   For example, a preference file reader might read prefs.arco and
   call the prefs_set_* functions in this module to make them known.
   Then, any Arco function can get a preference value by calling
   one of the prefs_* functions such as prefs_in_chans().
 
   Reading/writing/managing preferences is separated from access
   to preferences so that different preference systems can be
   implemented. E.g. Serpent programs should use Serpent preferences
   (prefs.srp), while an Arco server with curses interface should
   implement reading/writing to a file and editing in curses.
 */

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "arcotypes.h"
#include "o2internal.h"
#include "config.h"

// Undefine Windows MOUSE_MOVED before including curses to avoid conflict
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif

#ifdef __linux__
#include "ncurses.h"
#else
#include "curses.h"
#endif

#include "prefs.h"
#include "fieldentry.h"
#include "termui.h"
#include "arco_internal.h"
#if USE_MIDI
  #include "portmidi.h"
  #include "midiservice.h"
#endif
#include "o2oscservice.h"

/* these are the preferences */
char p_in_name[80] = DFLT_IN_NAME;
char p_out_name[80] = DFLT_OUT_NAME;
int p_in_id = DFLT_IN_ID;
int p_out_id = DFLT_OUT_ID;
int p_in_chans = DFLT_IN_CHANS;
int p_out_chans = DFLT_OUT_CHANS;
int p_buffer_size = DFLT_BUFFER_SIZE;
int p_latency_ms = DFLT_LATENCY_MS;
char p_network_option[24] = DFLT_NETWORK_OPTION;
bool p_o2lite_enable = DFLT_O2LITE_ENABLE;
bool p_http_enable = DFLT_HTTP_ENABLE;  
int p_http_port = DFLT_HTTP_PORT;
char p_http_root[120] = DFLT_HTTP_ROOT;
char p_debug_flags[64] = DFLT_DEBUG_FLAGS;
int p_polling_rate = DFLT_POLLING_RATE;
char p_ensemble_name[34] = DFLT_ENSEMBLE_NAME;
bool p_reference_clock = DFLT_REFERENCE_CLOCK;
int p_mqtt_port = DFLT_MQTT_PORT;
char p_mqtt_host[120] = DFLT_MQTT_HOST;

static const char *pref_file_name = "arco_server_prefs.json";

ConfigManager configs;
Config *current_config = nullptr;

char *find_nonspace(const char *str)
{
    while (*str != '\n' && *str != 0 && isspace(*str)) str++;
    return (char *) str;
}


void trim_space(char *str)
{
    int len = (int) strlen(str) - 1;
    while (len >= 0 && isspace(str[len])) {
        str[len--] = 0;
    }
}


static bool get_number(const char *line, const char *key, int *value)
{
    if (strstr(line, key) != 0) {
        const char *pd = find_nonspace(line + strlen(key));
        *value = atoi(pd);
        return true;
    }
    return false;
}


void get_name(const char *line_ptr, char *p_device)
{
    char *pd = find_nonspace(line_ptr);
    bool is_quoted = false;
    if (*pd == '"') {
        is_quoted = true;
        pd++;
    }
    trim_space(pd);  // remove any trailing newline or space
    size_t len = strlen(pd);

    // otherwise remove existing final quote if quoted
    if (is_quoted && pd[len - 1] == '"') {
        pd[len - 1] = 0; 
    }
    strcpy(p_device, pd);
}

#include <errno.h>

bool server_set_debug_flags(const char *new_flags)
{
    if (strcmp(new_flags, prefs_debug_flags()) != 0) {
        o2_debug_flags(new_flags);
        prefs_set_debug_flags(new_flags);
        printf("*** Set debug flags: \"%s\".\n",
               prefs_debug_flags());
        return true;
    }
    return false;
}


void load_configurations(const char *default_configuration)
{
    configs.load(pref_file_name);
    if (default_configuration && configs.has_config(pref_file_name)) {
        current_config = &configs.config(default_configuration);
        // else find and use default name in __configuration__:
    } else if (configs.has_config("__configuration__")) {
        string dflt = configs.config("__configuration__").get_string_value(
                                                       "__configuration__");
        current_config = &configs.config(dflt);
    } else {
        current_config = &configs.get_first_config();
        configs.config("__configuration__").add("__configuration__",
                           current_config->get_string_value("name"));
    }
    printf("finished reading %s\n", pref_file_name);
}


// returns true if pref changes would require a restart
bool config_to_prefs()
{
    bool need_restart = false;

    string value = current_config->get_string_value("arco_in_name");
    if (value.size() > 0) prefs_set_in_name(value.c_str());

    value = current_config->get_string_value("arco_out_name");
    if (value.size() > 0) prefs_set_out_name(value.c_str());

    value = current_config->get_string_value("in_chans");
    if (value.size() > 0) prefs_set_in_chans(atoi(value.c_str()));

    value = current_config->get_string_value("out_chans");
    if (value.size() > 0) prefs_set_out_chans(atoi(value.c_str()));

    value = current_config->get_string_value("buffer_size");
    if (value.size() > 0) prefs_set_buffer_size(atoi(value.c_str()));

    value = current_config->get_string_value("latency");
    if (value.size() > 0) prefs_set_latency(atoi(value.c_str()));

    value = current_config->get_string_value("network_option");
    if (value.size() > 0) prefs_set_network_option(value.c_str());

    value = current_config->get_string_value("o2lite_enable");
    if (value.size() > 0) prefs_set_o2lite_enable(value == "T");

    value = current_config->get_string_value("debug_flags");
    if (value.size() > 0) server_set_debug_flags(value.c_str());

    value = current_config->get_string_value("ensemble");
    if (value.size() > 0) prefs_set_ensemble_name(value.c_str());

    value = current_config->get_string_value("polling_rate");
    if (value.size() > 0) prefs_set_polling_rate(atoi(value.c_str()));

    value = current_config->get_string_value("reference_clock");
    if (value.size() > 0) prefs_set_reference_clock(value == "T");

    value = current_config->get_string_value("http_root");
    if (value.size() > 0) prefs_set_http_root(value.c_str());

    value = current_config->get_string_value("http_port");
    if (value.size() > 0) prefs_set_http_port(atoi(value.c_str()));

    value = current_config->get_string_value("mqtt_host");
    if (value.size() > 0) prefs_set_mqtt_host(value.c_str());

    value = current_config->get_string_value("mqtt_port");
    if (value.size() > 0) prefs_set_mqtt_port(atoi(value.c_str()));

    prefs_set_http_enable(strlen(prefs_http_root()) > 0);
            
    if (strcmp(host_network_option, prefs_network_option()) != 0 ||
        host_o2lite_enable != prefs_o2lite_enable() ||
        strcmp(host_ensemble_name, prefs_ensemble_name()) != 0) {
        need_restart = true;
    }
    return need_restart;
}


// used when dialog says restore - restore from prefs
void prefs_to_config()
{
    current_config->set_value("arco_in_name", prefs_in_name());
    current_config->set_value("arco_out_name", prefs_out_name());
    current_config->set_value("in_chans", itos(prefs_in_chans()));
    current_config->set_value("out_chans", itos(prefs_out_chans()));
    current_config->set_value("buffer_size", itos(prefs_buffer_size()));
    current_config->set_value("latency", itos(prefs_latency_ms()));
    current_config->set_value("network_option", prefs_network_option());
    current_config->set_value("o2lite_enable",
                              prefs_o2lite_enable() ? "T" : "F");
    current_config->set_value("debug_flags", prefs_debug_flags());
    current_config->set_value("ensemble", prefs_ensemble_name());
    current_config->set_value("polling_rate", itos(prefs_polling_rate()));
    current_config->set_value("reference_clock",
                              prefs_reference_clock() ? "T" : "F");
    current_config->set_value("http_root", prefs_http_root());
    current_config->set_value("http_port", itos(prefs_http_port()));
    current_config->set_value("mqtt_host", prefs_mqtt_host());
    current_config->set_value("mqtt_port", itos(prefs_mqtt_port()));

    // to restore midi and osc input/output
    config_remove_midi_osc();

    // work from connections to restore configuration
    for (Osc_io_info &in_info : from_osc_input) {
        vector<string> values;
        values.push_back(itos(in_info.port));
        values.push_back(in_info.is_tcp ? "T" : "F");
        values.push_back(in_info.service);
        current_config->add("osc_out", Config::list_to_string(values));
    }
    for (Osc_io_info &out_info : to_osc_output) {
        vector<string> values;
        values.push_back(out_info.service);
        values.push_back(out_info.ip);
        values.push_back(itos(out_info.port));
        values.push_back(out_info.is_tcp ? "T" : "F");
        current_config->add("osc_out", Config::list_to_string(values));
    }
#if USE_MIDI
    for (Midi_io_info &in_info : from_midi_input) {
        vector<string> values;
        values.push_back(in_info.device_name);
        values.push_back(in_info.address_or_service);
        current_config->add("midi_in", Config::list_to_string(values));
    }
    for (Midi_io_info &out_info : to_midi_output) {
        vector<string> values;
        values.push_back(out_info.address_or_service);
        values.push_back(out_info.device_name);
        current_config->add("midi_out", Config::list_to_string(values));
    }
#endif
}


void prefs_write()
{
    configs.save(pref_file_name);
    printf("Wrote %s\n", pref_file_name);
}


char *prefs_in_name()
{
    return p_in_name;
}


char *prefs_out_name()
{
    return p_out_name;
}

/*
// call this with each audio device number and name,
// formatted as "<number> - <name> (<in/out>)"
// The name is truncated at 79 characters
void prefs_id_lookup(const char *info)
{
    char name[80];
    // extract number:
    int id = atoi(info);
    // extract name:
    const char *start = strstr(info, " - ");
    const char *end = strrchr(info, '(');
    if (!start || !end) return;  // just in case

    // copy name from info so we can use strstr to search:
    start += 3;
    end -= 1;
    if ((end - start) >= 80) {
        end = start + 79;
    }
    strncpy(name, start, end - start);
    name[end - start] = 0; // EOS

    // see if info contains in_name or out_name:
    if (strstr(name, p_in_name)) {
        prefs_set_in_id(id);
    }
    if (strstr(name, p_out_name)) {
        prefs_set_out_id(id);
    }
}
 */


int lookup_device_id(string name)
{
    if (name == "") {
        return -1;
    }
    for (string &info : arco_device_info) {
        if (info.find(name) != std::string::npos) {
            return stoi(info);
        }
    }
    return -1;
}


int prefs_in_id()
{
    return lookup_device_id(prefs_in_name());
}

int prefs_out_id()
{
    return lookup_device_id(prefs_out_name());
}

int prefs_in_chans() { return p_in_chans != -1 ? p_in_chans : 2; }

int prefs_out_chans() { return p_out_chans != -1 ? p_out_chans : 2; }

int prefs_buffer_size() { return p_buffer_size != -1 ? p_buffer_size : BL; }

int prefs_latency_ms() { return p_latency_ms != -1 ? p_latency_ms : 10; }

const char *prefs_network_option()
{
    return p_network_option[0] ? p_network_option : "local network";
}

bool prefs_o2lite_enable() { return p_o2lite_enable; }

bool prefs_http_enable() { return p_http_enable; }

int prefs_http_port() { return p_http_port; }

const char *prefs_http_root() { return p_http_root; }

int prefs_mqtt_port() { return p_mqtt_port; }

const char *prefs_mqtt_host() { return p_mqtt_host; }

const char *prefs_debug_flags() { return p_debug_flags; }

const char *prefs_ensemble_name() { return p_ensemble_name; }

int prefs_polling_rate() { return p_polling_rate; }

bool prefs_reference_clock() { return p_reference_clock; }

void prefs_set_in_id(int id)
{
    prefs_set_in_name(arco_name_lookup(id));
}


void prefs_set_out_id(int id)
{
    prefs_set_out_name(arco_name_lookup(id));
}


void prefs_set_in_name(const char *name) 
{ 
    strncpy(p_in_name, name, 80);
    p_in_name[79] = 0;
}
    

void prefs_set_out_name(const char *name)
{ 
    strncpy(p_out_name, name, 80);
    p_out_name[79] = 0;
}
    

void prefs_set_latency(int latency_ms) { p_latency_ms = latency_ms; }
void prefs_set_in_chans(int chans) { p_in_chans = chans; }
void prefs_set_out_chans(int chans) { p_out_chans = chans; }
void prefs_set_buffer_size(int size) { p_buffer_size = size; }

void prefs_set_network_option(const char *option) {
    strncpy(p_network_option, option, sizeof(p_network_option));
    p_network_option[sizeof(p_network_option) - 1] = 0;
}

void prefs_set_o2lite_enable(bool enable) { p_o2lite_enable = enable; }

void prefs_set_http_enable(bool enable) { p_http_enable = enable; }

void prefs_set_http_port(int port) { p_http_port = port; }

void prefs_set_http_root(const char *root) {
    strncpy(p_http_root, root, sizeof(p_http_root));
    p_http_root[sizeof(p_http_root) - 1] = 0;
}

void prefs_set_mqtt_port(int port) { p_mqtt_port = port; }

void prefs_set_mqtt_host(const char *host) {
    strncpy(p_mqtt_host, host, sizeof(p_mqtt_host));
    p_mqtt_host[sizeof(p_mqtt_host) - 1] = 0;
}

void prefs_set_debug_flags(const char *flags) {
    strncpy(p_debug_flags, flags, sizeof(p_debug_flags));
    p_debug_flags[sizeof(p_debug_flags) - 1] = 0;
}

void prefs_set_ensemble_name(const char *ens) {
    strncpy(p_ensemble_name, ens, sizeof(p_ensemble_name));
    p_ensemble_name[sizeof(p_ensemble_name) - 1] = 0;
}

void prefs_set_polling_rate(int rate) { p_polling_rate = rate; }

void prefs_set_reference_clock(bool enable) { p_reference_clock = enable; }
