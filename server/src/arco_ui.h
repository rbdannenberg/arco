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
        bool *boolptr;
    } varptr;
};

extern Vec<O2string> arco_device_info;
extern int field_top;
extern bool arco_interrupt_requested;

int ui_init(int count);
int ui_poll(int delay_ms);
int ui_finish();

void ui_get_string(const char *prompt, void (*callback)(const char *s));

void ui_start_dialog(const char *title);
void ui_int_field(const char *prompt, int *value, int min, int max,
                  int actual, int dflt, int id);
void ui_bool_field(const char *prompt, bool *value,
                   bool actual, bool dflt, int id);
void ui_menu_field(const char *prompt, char *value, const char **options,
                   const char *actual, const char *pref, int id);
void ui_string_field(const char *prompt, char *value, int width,
                   const char *actual, const char *pref, int id);
void ui_run_dialog();

// to be provided by client:
extern const char *help_strings[];
extern const char *main_commands;

// to be implemented by client:
int action(int ch);
void configure_screen_finish();  // dialog manager action

