// fieldentry.cpp -- an object to manage one field in a curses form
//
// Roger B. Dannenberg
// Feb 2024

/* fieldentry allows users to fill in fields of a form.
The interaction is modal and assumes no curses interaction
except fieldentry_handle_typing until form interaction is complete
The life cycle is roughly:
- client configures curses.
- client can put up any text desired, and should clear areas
    of the screen where the form will go.
- Field_entry objects are created for each field. This results
    immediately in form display. There is no refresh while form
    is active.
- client calls fieldentry_handle_typing(ch) for each input
- when user types whatever exits the form (e.g., ESC key), the
    client can extract field data (this is done in ui_end_dialog() --
    see arco_ui.cpp).
- client should call delete_all_fields() to clean up (also in arco_ui.cpp).
*/

#include "assert.h"
//#include "o2.h"
//#include "portmidi.h"
#include "ctype.h"
#include "curses.h"
#include "string.h"
//#include "sys/stat.h"
#include <stdlib.h>
#include "fieldentry.h"

static int host_period = 2;
static int host_rate = 500;

static int required_height = 4;


Field_entry *fields = NULL;
Field_entry *insert_after = NULL;
Field_entry *last_field = NULL;
Field_entry *current_field = NULL;

int fe_xpos;
int fe_ypos;

void fe_move(int y, int x)
{
    move(y, x);
    fe_xpos = x;
    fe_ypos = y;
}


// configure -- handle keyboard input to set O2 configuration

#define DEL_CHAR 0x7f
#define BACKSPACE_CHAR 0x08

// find index of content in array of strings (options), return dflt if none
// are found
int string_list_index(const char **options, const char *content, int dflt)
{
    int i = 0;
    while (options[i]) {
        if (strcmp(options[i], content) == 0) {
            return i;
        }
        i++;
    }
    return dflt;
}


Field_entry::Field_entry(int label_x_, int x_, int y_, const char *label_,
                         int max_width_, int ftype, int id_, void *value_,
                         Field_entry *after)
{
    label_x = label_x_;
    label = label_;
    after_field = NULL;
    x = x_;
    y = y_;
    max_width = max_width_;
    assert(max_width <= MAX_FIELD_LEN);
    width = 0;
    content[0] = 0;
    if (field_type == FIELD_IP) {
        strcpy(content, "   .   .   .   ");
        width = 15;
    }

    options = NULL;
    field_type = ftype;
    allow_spaces = false;
    id = id_;
    value = value_;
    next = NULL;
    // insert this into list of fields
    if (after) {
        next = after->next;
        after->next = this;
    } else {
        if (!fields) {
            fields = this;
        } else {
            last_field->next = this;
        }
        last_field = this;
    }
}

void delete_all_fields()
{
    while (fields) {
        Field_entry *next = fields->next;
        delete fields;
        fields = next;
    }
    last_field = nullptr;
}


// preserve the current selection if it is in options. Otherwise,
// select the first option if there are any. Otherwise, set the
// selection to the empty string.
void Field_entry::set_field_to_option()
{
    int i = current_option(0);  // find if content matches an option
    set_option(i);
}


void Field_entry::set_menu_options(const char *options_[])
{
    options = options_;
    set_field_to_option();
}


// write the content, restore cursor to xpos, ypos
void Field_entry::show_content()
{
    int ypos = fe_ypos;
    int xpos = fe_xpos;

    fe_move(y, label_x);
    addstr(label);
    int pad_to = (is_ip() ? x + MAX_FIELD_LEN : x);
    // pad with blanks after label to field's start x:
    for (int i = label_x + (int) strlen(label); i < pad_to; i++) {
        addstr(" ");
    }
    fe_move(y, x);
    attron(A_UNDERLINE);
    addstr(content);
    for (int i = x + width; i < x + max_width; i++) {
        addstr(" ");  // pad with blanks to erase previous text
    }
    attroff(A_UNDERLINE);
    if (after_field) {
        addstr(after_field);
    }
    fe_move(ypos, xpos);  // restore cursor
}


void Field_entry::handle_typing(int ch)
{
    // do not allow typing if this is multiple choice field
    if (!options && cursor_in_or_after_field()) {
        if (ch == DEL_CHAR) {
            if (fe_xpos <= x) {
                ;  // ignore DEL if you are at the beginning of field
            } else if (is_ip()) {
                // delete in IP address edits a single byte
                int loc = fe_xpos - x - 1;  // location to delete
                if (content[loc] == '.') {
                    ;  // if delete is after '.', then ignore
                } else {  // shift from right; pad with blank
                    fe_xpos = x + loc;
                    if (loc % 4 == 0) {
                        content[loc] = content[loc + 1];
                        loc++;
                    }
                    if (loc % 4 == 1) {
                        content[loc] = content[loc + 1];
                        loc++;
                    }
                    if (loc % 4 == 2) {
                        content[loc] = ' ';
                    }
                    show_content();
                }
            } else if (width > 0) {
                strcpy(content + fe_xpos - x - 1, content + fe_xpos - x);
                width--;
                fe_xpos--;
                show_content();
            } // else ignore DEL
        } else if (is_int() && !isdigit(ch) &&
                   (!allow_spaces || ch != ' ')) {
            ;  // ignore non-digits if field is an integer (also for is_ip)
        } else if (is_button()) {
            if (ch == 'y' || ch == 'Y' || ch == 'x' || ch == 'X') {
                do_command(this);
            }
        } else if (is_bool()) {
            if (ch == 'y' || ch == 'Y' || ch == 't' || ch == 'T') {
                content[0] = 'T';
                content[1] = 0;
                show_content();
            } else if (ch == 'n' || ch == 'N' || ch == 'f' || ch == 'F') {
                content[0] = 'F';
                content[1] = 0;
                show_content();
            } else {
                return;  // ignore non-bool typing
            }
        } else if ((fe_xpos - x <= max_width) &&
                   (allow_spaces || ch != ' ')) {
            content[fe_xpos - x] = ch;
            content[fe_xpos + 1 - x] = 0;  // EOS
            attron(A_UNDERLINE);
            mvaddch(fe_ypos, fe_xpos, ch);
            attroff(A_UNDERLINE);
            fe_xpos++;
            width = (int) strlen(content);
            int loc = fe_xpos - x;
            if (is_ip() && (loc == 3 || loc == 7 || loc == 11)) {
                fe_xpos++;    // skip over '.'s
            }
            fe_move(fe_ypos, fe_xpos);
            refresh();
        }
    }
}


int Field_entry::current_option(int dflt)
{
    return string_list_index(options, content, dflt);
}


void Field_entry::set_content(const char *s)
{
    strncpy(content, s, max_width);
    width = (int) strlen(content);
}


// used to transfer configuration data to the display when
// the stored data is an int rather than a string. Zero
// value is a special case that is displayed as if_zero,
// which is normally either "" or "0", e.g. we encode
// unspecified port numbers as 0, so we want to display
// "unspecified" with the empty string "".
void Field_entry::set_number(int i, const char *if_zero)
{
    if (i == 0) {
        strncpy(content, if_zero, max_width);
    } else {
        snprintf(content, max_width, "%d", i);
    }
    width = (int) strlen(content);
}


void Field_entry::set_option(int i)
{
    if (!options[i]) {  // NULL option -> empty string
        content[0] = 0;
    } else {
        strncpy(content, options[i], max_width);
        content[max_width] = 0;
    }
    width = (int) strlen(content);        
}


void Field_entry::next_option()
{
    if (!options) {
        return;  // ignore command if not an option
    }
    int i = current_option(-1) + 1;
    // note that if content did not match any option, then
    // i will not be zero, the first option, which is a good choice
    if (!options[i]) {
        i = 0;  // wrap to first option
    }
    set_option(i);
    show_content();
}

Field_entry *Field_entry::save(FILE *outf, const char *prefix, bool newline)
{
    fprintf(outf, "%s \"%s\"%s", prefix, content, newline ? "\n" : "");
    return next;
}

void Field_entry::prev_option()
{
    int i = current_option(-1) - 1;
    if (i == -1) {  // wrap to last
        while (options[i + 1]) i++;
    } else if (i == -2) {  // content did not match any option
        i = 0;  // set to first option
    }
    set_option(i);
    show_content();
}

// cursor is on some text of this field
bool Field_entry::cursor_in_field_text()
{
    return fe_xpos >= x && fe_xpos < x + width && fe_ypos == y;
}

// cursor is to the right of any existing text (1 to len)
bool Field_entry::cursor_after_field_text()
{
    return fe_xpos > x && fe_xpos <= x + width && fe_ypos == y;
}

// cursor is within this field
bool Field_entry::cursor_in_or_after_field()
{
    return fe_xpos >= x && fe_xpos <= x + max_width && fe_ypos == y;
}

void fieldentry_handle_typing(int ch)
{
    if (!current_field) {
        if (!fields) {
            return;
        }
        set_current_field(fields);
    }
    if (ch == '\t' || ch == '\n') {
        tab_to_field();
   } else if (ch == KEY_LEFT || ch == KEY_RIGHT ||
              ch == KEY_UP || ch == KEY_DOWN) {
       if (current_field->options) {
           if (ch == KEY_DOWN) {
               current_field->next_option();
           } else if (ch == KEY_UP) {
               current_field->prev_option();
           } else if (ch == KEY_RIGHT) {
               tab_to_field();
           } else if (ch == KEY_LEFT) {
               move_to_end_of_previous_field();
           }
       } else {
           if (ch == KEY_RIGHT) {
               if (current_field->cursor_in_field_text()) {
                   fe_move(fe_ypos, fe_xpos + 1);
               } else {
                   tab_to_field();  // go to next field
               }
           } else if (ch == KEY_LEFT) {
               if (current_field->cursor_after_field_text()) {
                   fe_move(fe_ypos, fe_xpos - 1);
               } else {
                   move_to_end_of_previous_field();
               }
           } else if (ch == KEY_UP) {
               move_to_line(-1);
           } else if (ch == KEY_DOWN) {
               move_to_line(1);
           }
       }
    } else if (ch == DEL_CHAR || ch == BACKSPACE_CHAR || ch == KEY_DC) {
        current_field->handle_typing(DEL_CHAR);
    } else if (ch <= 0 || ch >= 128) {
        return;  // ignore other "special" characters
    } else if (isgraph(ch) || ch == ' ') {
        current_field->handle_typing(ch);
    }
}


// draw all (empty) fields:
void draw_all_fields()
{
    for (Field_entry *field = fields; field; field = field->next) {
        field->show_content();
    }
}


void set_current_field(Field_entry *field)
{
    current_field = field;
    // current_field could be NULL, either because it started that way or
    // we reached the end of the fields list: either way, move to first field:
    if (!current_field) {
        current_field = fields;  // first one
    }
    if (current_field) {
        fe_move(current_field->y, current_field->x);
        wrefresh(stdscr);
    }
}


// delete_or_insert - remove or add an optional service to/from midi/osc
//     connection.
// y - the line to be removed or the location of the new line
// inc - +1 if insert, -1 if deleting an option.
// The effect is to shift the lines below y up or down according to inc.
//
// Returns true for redraw request.
//
bool delete_or_insert(int y, int inc)
{
    required_height += inc;
    // adjust all fields that changed lines
    for (Field_entry *field = fields; field; field = field->next) {
        if (inc == -1 && field->y > y) {
            field->y--;
        } else if (inc == 1 && field->y >= y) {
            field->y++;
        }
    }
    return true;
}


void move_to_line(int direction)
{
    if (!current_field) {
        current_field = fields;
    }
    if (!current_field) {
        return;
    }
    int y = current_field->y + direction;
    while (true) {  // if no other line has a field, this will wrap
                    // around to the first field in the current_field's line
        for (Field_entry *field = fields; field; field = field->next) {
            if (field->y == y) {
                set_current_field(field);
                return;
            }
        }
        y = (y + direction + LINES) % LINES;
    }
}


void tab_to_field()
 {
    // find field after this one (wraps around at end)
    if (current_field) {
        set_current_field(current_field->next);  // handles wrapping
    }
}


void move_to_end_of_previous_field()
{
    Field_entry *field;
    for (field = fields; field->next && field->next != current_field;
         field = field->next) {
        ;
    }
    set_current_field(field);
}
