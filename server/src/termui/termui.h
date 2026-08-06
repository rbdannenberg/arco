/* termui.h -- implement a small UI and terminal output in one terminal
 *
 * Roger B. Dannenberg
 * Jul 2026
 */

#define TUDBG(x)

typedef void (*field_callback_t)(string key, int ch);
typedef void (*key_callback_t)(int key, bool is_escape);
typedef void (*string_callback_t)(string s);

/*
// stops polling so we can clean up when Ctrl-C is typed
extern int termui_interrupt_requested;
*/

class Terminal_ui {
  protected:
    FILE *ttyfd;
    TUDBG(FILE *logfile;)  // for debugging

    int out_pipe[2];
    // WIN32 only for now:
    int orig_stdout_fd;
    int orig_stderr_fd;

    int save_out;
    int save_err;

    bool direct_mode;
    bool help_mode;
    bool dialog_mode;

    vector<string> *top_lines;
    vector<string> *bottom_lines;
    int lines_max;
    vector<string *> lines;
    int display_index;  // what line is top of display

    string curline;

    // cursor location when writing std output to screen
    int out_line;
    int out_col;

    // dialog
    bool dialog_under_construction;
    bool dialog_completed;
    bool getting_string;
    Field_entry *current_field;
    string escape_keys;  // keys that can be used to exit dialog
    string_callback_t string_callback;

    // help
    vector<string> help_strings;
    string help_command_keys;

  public:
    vector<Field_entry *> fields;
    int dialog_x;  // location of cursor during dialog processing
    int dialog_y;

    key_callback_t key_callback;
    field_callback_t field_callback;

    SCREEN *uiscr;  // if NULL, initialization faile

    Terminal_ui(int count);  // how many lines to save
    int finish();  // return to normal screen
    
    void fixed_info(vector<string> *top_lines, vector<string> *bottom_lines);

    int field_int(string label, string key, string init, int min, int max,
                  int x, int y, int w1, int w2);
    int field_double(string label, string key, string init,
                    double min, double max, int x, int y, int w1, int w2);
    int field_string(string label, string key, string init,
                     int x, int y, int w1, int w2);
    int field_ip(string label, string key, string init,
                 int x, int y, int w1, int w2);
    int field_bool(string label, string key, string init,
                   int x, int y, int w1, int w2);
    int field_button(string label, string key,
                     int x, int y, int w1, int w2);
    int field_menu(string label, string key, string init,
                   int x, int y, int w1, int w2, vector<string> &options);
    int field_blank(string key, int y);
    void set_menu_options(string key, vector<string> &options, int n);
    void dialog_escape_keys(string keys);
    void set_field_callback(field_callback_t callback);
    void set_key_callback(key_callback_t callback);
    void dialog_begin();
    void dialog_end(int key);
    void dialog_run();

    bool has_value(string key);
    int get_int(string key, int dflt);
    double get_double(string key, double dflt);
    bool get_bool(string key, bool dflt);
    string get_string(string key, string dflt);
    string get_ip(string key, string dflt);
    int get_menu_index(string key);

    int find_field(string key);
    void set_current_field(Field_entry *field);
    void dialog_insert_line(int n);
    void dialog_remove_line(int n);
    void dialog_move_field(int i, string location_key, bool before = false);
    void dialog_refresh();

    void get_string(const char *prompt, string_callback_t callback);

    void add_help(string help_string);
    void clear_help();
    void help_keys(string keys);

    int top_size();  // number of fixed lines at top
    int bottom_size();  // number of fixed lines at bottom
    void poll(int delay_usec);

  protected:
    // implementation
    void fields_delete_entry(Field_entry *entry);
    void delete_all_fields();
    void tab_to_field();
    void move_to_previous_field();
    void dialog_handle_typing(int key);
    void dialog_draw();
    void move_to_line(int direction);
    void advance(int n);
    void body_clear();
    void show_help();
    void output_line();  // outputs curline
    void output_char(char c);
    void newline(int maxx, int maxy);
    void output(char *buffer);
    void screen_refresh();
    void got_a_char(int ch);
};


string format_ip_content(const string &input);
string content_to_ip(const string &input);
