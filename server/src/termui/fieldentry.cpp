// fieldentry.cpp -- an object to manage one field in a curses form
//
// Roger B. Dannenberg
// Feb 2024, updated July 2026

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
    client can extract field data (this is done in ui_dialog_end() --
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
#include <string>
#include <vector>

using std::string;
using std::vector;
#include "fieldentry.h"
#include "termui.h"

static int host_period = 2;
static int host_rate = 500;

static int required_height = 4;


#define streql(x, y) (strcmp(x, y) == 0)


// configure -- handle keyboard input to set O2 configuration

// MacOS: backspace appears as 0x7F, Fn-Delete (forward delete) is 330
// Windows: backspace appears as 0x08, Delete (forward) is 330
#ifdef WIN32
#define BACKSPACE_CHAR 0x08
#else
#define BACKSPACE_CHAR 0x7f
#endif

// find index of content in array of strings (options), return dflt if none
// are found
int string_vector_index(vector<string> &options, string &content, int dflt)
{
    int i = 0;
    for (string &option : options) {
        if (option == content) {
            return i;
        }
        i++;
    }
    return dflt;
}


Field_entry::Field_entry(string label_, string key_, string init, double min_,
                     double max_, int x_, int y_, int w1_, int w2_, int ftype)
{
    label = label_;
    key = key_;
    min = min_;
    max = max_;
    x = x_;
    y = y_;
    w1 = w1_;
    w2 = w2_;
    field_type = ftype;
    content = init;
    if (field_type == FIELD_IP && content.size() == 0) {
        content.insert(0, "___.___.___.___");
    }
    options = NULL;
    allow_spaces = false;
}


// write the content, restore cursor to xpos, ypos
void Field_entry::show_content(Terminal_ui *tu)
{
    move(y, x);
    addstr(label.c_str());
    // pad with blanks after label to field's start x:
    for (int i = (int) label.size(); i < w1 + 1; i++) {
        addstr(" ");
    }
    if (field_type == FIELD_BUTTON) {
        addch(ACS_CKBOARD);
    } else if (field_type == FIELD_BOOL) {
        const char *str = "[X]";
        if (content == "" || content == "F") {
            str = "[ ]";
        }
        addstr(str);
    } else {
        addstr(content.c_str());
        for (int i = (int) content.size(); i < w2; i++) {
            // Using A_UNDERLINE was not reliable -- they blink on and off with
            // cursor movement, so just use underlines instead
            addstr("_");  // pad with blanks to erase previous text
        }
    }
    move(tu->dialog_y, tu->dialog_x);  // restore cursor
}


void Field_entry::handle_typing(int ch, Terminal_ui *tu)
{
    if (options) {
        if (ch == KEY_UP) {
            prev_option(tu, ch);
        } else if (ch == KEY_DOWN) {
            next_option(tu, ch);
        }  // otherwise, do nothing with the key in an option field
    } else if (cursor_in_or_after_field(tu->dialog_x, tu->dialog_y)) {
        int content_posn = tu->dialog_x - (x + w1 + 1);
        // Fake delete char by "moving" cursor and doing a backspace delete.
        // If it fails, we didn't really move the cursor, so nothing to clean up
        // but if it is successful, DEL_CHAR moves the cursor to the left, so
        // we have to remember not to do that:

        if ((ch == BACKSPACE_CHAR || ch == KEY_DC) && field_type != FIELD_BOOL) {
            int loc = content_posn;  // location to delete
            if (ch == BACKSPACE_CHAR) {
                loc--;  // delete location is left of cursor posn
            }
            if (loc < 0) {
                ;  // ignore DEL if you are at the beginning of field
                   // never happens with KEY_DC because content_posn incremented
            } else if (is_ip()) {
                // delete in IP address edits a single byte (1 of 4 fields)
                if (content[loc] == '.') {
                    ;  // if delete is after '.', then ignore
                } else {  // shift from right; pad with blank
                    if (ch == BACKSPACE_CHAR) {  // only move cursor if we had DEL_CHAR
                        tu->dialog_x = x + w1 + loc;
                    }
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
                    show_content(tu);
                }
            } else if (content.size() > 0) {
                if (ch == BACKSPACE_CHAR) {  // only move cursor if we have backspace
                    tu->dialog_x--;
                }
                content.erase(loc, 1);
                show_content(tu);
            } // else ignore DEL
        } else if ((is_int() || is_ip()) && !isdigit(ch)) {
            ;  // ignore non-digits if field is an integer
        } else if (is_button()) {
            if (strchr("yYxXtT", ch)) {
                if (tu->field_callback != nullptr) {
                    (*(tu->field_callback))(key, 0);
                }
            }
        } else if (is_bool()) {
            if (content != "T" && strchr("yYtTxX", ch)) {
                content = "T";
                show_content(tu);
            } else if (content != "F" && strchr("nNfFxX", ch)) {
                content = 'F';
                show_content(tu);
            } else {
                return;  // ignore non-bool typing
            }
        } else if (is_ip()) {
            int loc = content_posn;
            int base = loc & ~3;  // round to multiple of 4
            int offset = loc % 4;
            if (offset != 3 && content[base + 2] == '_') {  // can insert
                string update = content.substr(base, 3);
                update.insert(offset, 1, static_cast<char>(ch));
                // now update has length 4 with extra space at end
                content.replace(base, 3, update.substr(0, 3));
                tu->dialog_x += (offset == 2 ? 2 : 1);
            }
            show_content(tu);
        } else if ((content_posn < w2) &&
                   (allow_spaces ||  // either spaces allowed or type non-space
                                     // into or at end of existing string:
                    (ch != ' ' && (content_posn <= content.size())))) {
            if (field_type == FIELD_INT || field_type == FIELD_DOUBLE) {
                string test = content;
                test.insert(content_posn, 1, static_cast<char>(ch));
                if (test.size() >= w2) {
                    test.resize(w2, ' ');
                }
                double val = atof(test.c_str());
                if (val < min || val > max) {
                    return;  // ignore changes that would go outside range
                }
            }
            content.insert(content_posn, 1, static_cast<char>(ch));
            if (content.size() >= w2) {
                content.resize(w2, ' ');  // should never pad, but be safe
            }
            tu->dialog_x++;
            show_content(tu);
        }
    }
}


int Field_entry::current_option()
{
    return string_vector_index(*options, content, 0);
}


void Field_entry::set_content(string s)
{
    content = s;
}

/*
void Field_entry::set_number(int i, const char *if_zero)
{
    if (i == 0) {
        content = if_zero;
        // limit copy to width of content
        content.assign(if_zero, 0, strnlen(if_zero, w2));
    } else {
        string full_number = to_string(i);
        content.assign(full_number, 0, min(full_number.size(), w2));
    }
}
*/


void Field_entry::set_option(Terminal_ui *tu, int i, int ch)
{
    content = (*options)[i];
    show_content(tu);  // important to show_content() now because the field
            // exists, after callback(), it could be removed or replaced
    if (tu->field_callback) {
        (*tu->field_callback)(key, ch);
    }

}


void Field_entry::next_option(Terminal_ui *tu, int ch)
{
    if (options->size() == 0) {
        return;  // ignore command if no options
    }
    int i = (current_option() + 1) % options->size();
    set_option(tu, i, ch);
    // see note in prev_option() below about show_content()
}


/*
void Field_entry::save(FILE *outf, bool newline)
{
    fprintf(outf, "%s: \"%s\"%s", label, content, newline ? "\n" : "");
}
*/

void Field_entry:: prev_option(Terminal_ui *tu, int ch)
{
    if (options->size() == 0) {
        return;  // ignore command if no options
    }
    int i = current_option() - 1;
    if (i == -1) {  // wrap to last
        i = (int) options->size() - 1;
    } else if (i == -2) {  // content did not match any option
        i = 0;  // set to first option
    }
    set_option(tu, i, ch);  // this will probably reload dialog through callback
    // someone should do a show_content() if this Field_entry still exists, but
    // we cannot do it here because this may be freed by the callback.
}


// cursor is in or just right of the text of this field
bool Field_entry::cursor_in_field_text(int cx, int cy)
{
    return cx >= x + w1 && cx <= x + w1 + (int) content.size() && cy == y;
}


// cursor is to the right of any existing text character (1 to len)
bool Field_entry::cursor_after_field_text(int cx, int cy)
{
    return cx > x + w1 + 1 && cx <= x + w1 + 1 + content.size() && cy == y;
}

// cursor is within this field
bool Field_entry::cursor_in_or_after_field(int cx, int cy)
{
    return cx >= x + w1 + 1 && cx <= x + w1 + 1 + w2 && cy == y;
}

