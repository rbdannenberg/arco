/* arco_ui.cpp -- implement a small UI and terminal output in one terminal
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

struct Dminfo {
    char type;  // 's', 'i', or 'f'
    const char *label;
    char comment[64];
    bool changed;
    union {
        int *intptr;
        float *floatptr;
        char *charptr;
    } varptr;
};

extern Vec<Dminfo> dminfo;
extern Vec<O2string> arco_device_info;

int ui_init(int count);
int ui_poll(int delay_ms);
int ui_finish();

int ui_get_string(const char *prompt, void (*callback)(const char *s));

void ui_start_dialog();
int ui_int_field(const char *prompt, int *value, int min, int max,
                 int actual, int dflt);
void ui_run_dialog(const char *title);

// to be provided by client:
extern const char *help_strings[];
extern const char *main_commands;

// to be implemented by client:
int action(int ch);
void dmaction();  // dialog manager action

