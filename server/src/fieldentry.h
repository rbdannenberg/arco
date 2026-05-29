// fieldentry.h -- an object to manage one field in a curses form
//
// Roger B. Dannenberg
// Feb 2024

#include "assert.h"
//#include "o2.h"
//#include "portmidi.h"
#include "ctype.h"
#include "curses.h"
#include "string.h"
//#include "sys/stat.h"
//#include <unistd.h>

#define MAX_FIELD_LEN 32

#define FIELD_STRING 1
#define FIELD_INT 2
#define FIELD_BUTTON 3  /* display _ and call do_command() if x,X,y,Y typed */
#define FIELD_IP 4  /* display looks like ___.___.___.___ */
#define FIELD_BOOL 5  /* T/F field */
#define FIELD_MENU 6  /* list of selections */


class Field_entry {
  public:
    int label_x;
    int x;
    int y;
    const char *label;
    const char *after_field;  // put this text (if non-NULL) after field
    int max_width;  // does not include prompt or null terminator
    int width;      // equals strlen(content); does not include null terminator
    char content[MAX_FIELD_LEN + 1];
    const char **options;
    int field_type;
/*
    bool is_integer;
    bool is_button;
    bool is_ip;  
*/
    bool allow_spaces;  // allow typing spaces into field
    int id;
    void *value;  // where to store value
    Field_entry *next;

    // label_x_ is where to put the label
    // x_ is where to put the field
    // if after is non-null, we insert this field after it in the list,
    // otherwise we insert this at the end of the list.
    Field_entry(int label_x_, int x_, int y_, const char *label_,
                int max_width_, int ftype, int id, void *value, 
                Field_entry *after);


    // preserve the current selection if it is in options. Otherwise,
    // select the first option if there are any. Otherwise, set the
    // selection to the empty string.
    void set_field_to_option();

    bool is_string() { return field_type == FIELD_STRING; }
    bool is_menu() { return field_type == FIELD_MENU; }
    bool is_int() { return field_type == FIELD_INT; }
    bool is_button() { return field_type == FIELD_BUTTON; }
    bool is_ip() { return field_type == FIELD_IP; }
    bool is_bool() { return field_type == FIELD_BOOL; }

    // set the field to be an option menu using list of options_, which
    // is an array of pointers to strings followed by a NULL pointer.
    void set_menu_options(const char *options_[]);

    // set the field to be an IP address (nnn.nnn.nnn.nnn notation)
    void set_ip();

    // write the content, restore cursor to xpos, ypos
    void show_content();

    // this is the current entry and ch was typed:
    void handle_typing(int ch);
    
    // get the index of the currently selected option
    int current_option(int dflt);
    
    // set content to a string value
    void set_content(const char *s);
    
    // set a numerical field to an integer
    void set_number(int i, const char *if_zero);

    // select option i
    void set_option(int i);

    // select the next option
    void next_option();
    
    // save a field to (preference) file
    Field_entry *save(FILE *outf, const char *prefix, bool newline);

    // select the previous option
    void prev_option();
    
    // cursor is on some text of this field
    bool cursor_in_field_text();

    // cursor is to the right of any existing text (1 to len)
    bool cursor_after_field_text();

    // cursor is within this field
    bool cursor_in_or_after_field();
};

extern Field_entry *fields;
extern Field_entry *current_field;
extern Field_entry *insert_after;

// delete all fields
void delete_all_fields();

// process user input
void fieldentry_handle_typing(int ch);

// callout from Field_entry::handle_typing, defined in o2host.cpp
void do_command(Field_entry *field);

// find index of content in options
int string_list_index(const char **options, const char *content, int dflt);

// draw all (empty) fields:
void draw_all_fields();

// make field be the current field to edit
void set_current_field(Field_entry *field);

// delete (inc = -1) or insert (inc = +1) a line, moving all fields
// appropriately
bool delete_or_insert(int y, int inc);

// advance to a field on the next (+1) or previous (-1) line
void move_to_line(int direction);

// advance to the next field
void tab_to_field();

// go to the end of the previous field:
void move_to_end_of_previous_field();

