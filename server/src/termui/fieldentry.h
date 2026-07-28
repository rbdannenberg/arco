// fieldentry.h -- an object to manage one field in a curses form
//
// Roger B. Dannenberg
// Feb 2024, updated July 2026

class Terminal_ui;

#define MAX_FIELD_LEN 32

#define FIELD_STRING 1
#define FIELD_INT 2
#define FIELD_DOUBLE 3
#define FIELD_BUTTON 4  /* display _ and call do_command() if x,X,y,Y typed */
#define FIELD_BOOL 5  /* T/F field */
#define FIELD_IP 6  /* display looks like ___.___.___.___ */
#define FIELD_MENU 7  /* list of selections */

char *itos(int i);

class Field_entry {
  public:
    string label;
    string key;
    double min;
    double max;
    int x;
    int y;
    int w1;
    int w2;

    int width;      // equals strlen(content); does not include null terminator
    string content;
    vector<string> *options;  // array of options that FIELD_MENU can display
    int field_type;

    bool allow_spaces;  // allow typing spaces into field

    Field_entry(string label, string key, string init, double min, double max,
                int x, int y, int w1, int w2, int ftype);

    bool is_string() { return field_type == FIELD_STRING; }
    bool is_menu() { return field_type == FIELD_MENU; }
    bool is_int() { return field_type == FIELD_INT; }
    bool is_button() { return field_type == FIELD_BUTTON; }
    bool is_ip() { return field_type == FIELD_IP; }
    bool is_bool() { return field_type == FIELD_BOOL; }

    // set the field to be an option menu using list of options_, which
    // is an array of pointers to strings followed by a NULL pointer.
    void set_menu_options(vector<string> *options);

    // write the content, restore cursor to xpos, ypos
    void show_content(Terminal_ui *tu);

    // this is the current entry and ch was typed:
    void handle_typing(int ch, Terminal_ui *tu);
    
    // get the index of the currently selected option
    int current_option();
    
    // set content to a string value
    void set_content(string s);
    
    // select option i
    void set_option(Terminal_ui *tu, int i, int ch);

    // select the next option
    void next_option(Terminal_ui *tu, int ch);
    
    // select the previous option
    void prev_option(Terminal_ui *tu, int ch);
    
    // cursor is on some text of this field
    bool cursor_in_field_text(int cx, int cy);

    // cursor is to the right of any existing text (1 to len)
    bool cursor_after_field_text(int cx, int cy);

    // cursor is within this field
    bool cursor_in_or_after_field(int cx, int cy);

    // write to preference file, optionally write newline
    // void save(FILE *outf, bool newline);
};


// find index of content in options
int string_vector_index(vector<string> &options, string &content, int dflt);
