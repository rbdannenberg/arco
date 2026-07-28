/* termui.cpp -- implement a small UI and terminal output in one terminal
 *
 * Roger B. Dannenberg
 * July 2026
 */

// lines - a vector of output lines to allow scrolling back
// lines_max - the maximum number of retained text lines
// display_index - lines[display_index] is the first line displayed
//    in the scrolling output
//
// There are 2 modes for scrolling: direct_mode is true if output is
// written immediately to the screen; direct_mode is false if the
// users scrolls back and new output is just captured in lines[].
// If not direct_mode, dialog_mode, or help_mode, input is used to
// scroll the text output.
//
// When we hit lines_max lines, the first is freed, lines are shifted
// and a new line is added to the end. display_index is updated based
// on whether scrolling takes place and whether lines are shifted.
//
// display_index is never shifted below 0.
//
// When lines is full (size hits lines_max), new input causes old
// lines to be forgotten history, so if you have scrolled back to
// the beginning of lines (display_index is 0), the display will
// appear to scroll, even though we are not seeing old output from
// history.
//
// There's also dialog_mode, where scrolling stops and a form or help
// can be drawn.
//
// In dialog_mode, input is diverted to fieldentry_handle_typing unless
// help_mode, in which case any typing reverts to direct_mode.


#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include <ctype.h>
#include <assert.h>
#include <curses.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

using std::string;
using std::vector;

#include "fieldentry.h"
#include "termui.h"


#define ASCII_ESC 27

/* termui_interrupt_requested = false; */

Terminal_ui::Terminal_ui(int count)  // count is how many output lines to save
{
    ttyfd = NULL;
    TUDBG(logfile = NULL;)  // for debugging
    uiscr = NULL;

    direct_mode = true;
    help_mode = false;
    dialog_mode = false;

    // lines is a vector of strings, initially empty
    lines_max = count;
    display_index = 0;  // what line is top of display

    // cursor location when writing std output to screen
    out_line = 0;
    out_col = 0;

    // dialog state
    dialog_under_construction = false;
    dialog_completed = false;
    getting_string = false;
    current_field = nullptr;
    escape_keys = "";
    field_callback = nullptr;

    dialog_x = 0;
    dialog_y = 0;

    clear_help();

    help_command_keys = "H";

    TUDBG(logfile = fopen("log.txt", "w");)
    ttyfd = fopen("/dev/tty", "r+");
    if (!ttyfd) {
        printf("Could not open /dev/tty. Initialization Failed!\n");
        return;  // fail
    }
    struct termios t;
    int fd = fileno(ttyfd);
    tcgetattr(fd, &t);
    // clear the IXON flag to turn off CTRL-S/CTRL-Q flow control
    t.c_iflag &= ~IXON;
    tcsetattr(fd, TCSANOW, &t);
    // setenv("NCURSES_NO_PADDING", "1", 1);
    uiscr = newterm(NULL, ttyfd, ttyfd);
    set_term(uiscr);
    // ceol_standout_glitch = FALSE;  // disables an optimization that might
    // clear underline attributes in fields.
    // clearok(stdscr, FALSE);  // avoid hard-clear optimization, again, that
    // might cause clearing of underline attributes.
    nodelay(stdscr, TRUE);
    set_escdelay(50);
    noecho();
    curs_set(2);

    keypad(stdscr, true);
    save_out = dup(fileno(stdout));
    save_err = dup(fileno(stderr));
    pipe(out_pipe);
    dup2(out_pipe[1], fileno(stdout));
    dup2(out_pipe[1], fileno(stderr));

    screen_refresh();
}


int Terminal_ui::finish()
{
    if (!uiscr) {
        return -1;  // fail
    }
    endwin();
    delscreen(uiscr);
    fclose(ttyfd);
    fflush(stdout);
    fflush(stderr);
    dup2(save_out, fileno(stdout));
    dup2(save_err, fileno(stderr));
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(save_out);
    close(save_err);
    for (int i = 0; i < lines.size(); i++) {
        if (lines[i]) {
            delete lines[i];
        }
    }
    TUDBG(fclose(logfile);)
    return 0;
}


void Terminal_ui::fixed_info(vector<string> *top, vector<string> *bottom)
{
    top_lines = top;
    bottom_lines = bottom;
    screen_refresh();
}


int Terminal_ui::field_int(string label, string key, string init,
                           int min, int max, int x, int y, int w1, int w2)
{
    fields.push_back(new Field_entry(label, key, init, min, max,
                                     x, y , w1, w2, FIELD_INT));
    return (int) fields.size() - 1;
}


int Terminal_ui::field_double(string label, string key, string init,
                double min, double max, int x, int y, int w1, int w2)
{
    fields.push_back(new Field_entry(label, key, init, min, max,
                                     x, y , w1, w2, FIELD_DOUBLE));
    return (int) fields.size() - 1;
}


int Terminal_ui::field_string(string label, string key, string init,
                              int x, int y, int w1, int w2)
{
    fields.push_back(new Field_entry(label, key, init, 0, 0,
                                     x, y , w1, w2, FIELD_STRING));
    return (int) fields.size() - 1;
}


string format_ip_content(const string& input) {
    std::stringstream ss(input);
    string segment;
    string result = "";
    
    // Process up to 4 segments separated by periods
    for (int i = 0; i < 4; ++i) {
        string digits = "";
        
        // Extract the next token if it exists
        if (std::getline(ss, segment, '.')) {
            // Truncate to a maximum of 3 characters
            digits = segment.substr(0, 3);
        }
        
        // Pad on the left with underscores if it has fewer than 3 characters
        if (digits.length() < 3) {
            digits = string(3 - digits.length(), '_') + digits;
        }
        
        // Build the final string, appending periods between segments
        result += digits;
        if (i < 3) {
            result += ".";
        }
    }
    return result;
}


// convert ddd._dd.__d.___ from content into ddd.dd.d.0
string content_to_ip(const string& input)
{
    string result = "";
    bool has_digit = false;

    for (char c : input) {
        if (isdigit(c)) {
            result += c;
            has_digit = true;
        } else if (c == '.') {
            if (!has_digit) {
                result += '0';
            }
            result += '.';
            has_digit = false;
        }
    }
    if (!has_digit) {
        result += '0';  // final segment gets a digit
    }
    return result;
}


int Terminal_ui::field_ip(string label, string key, string init,
                          int x, int y, int w1, int w2)
{
    // format init to ip format ___.___.___.___
    init = format_ip_content(init);
    fields.push_back(new Field_entry(label, key, init, 0, 0,
                                     x, y , w1, w2, FIELD_IP));
    return (int) fields.size() - 1;
}


int Terminal_ui::field_bool(string label, string key, string init,
                            int x, int y, int w1, int w2)
{
    fields.push_back(new Field_entry(label, key, init, 0, 0,
                                     x, y , w1, w2, FIELD_BOOL));
    return (int) fields.size() - 1;
}


int Terminal_ui::field_button(string label, string key,
                              int x, int y, int w1, int w2)
{
    fields.push_back(new Field_entry(label, key, "", 0, 0,
                                     x, y , w1, w2, FIELD_BUTTON));
    return (int) fields.size() - 1;
}


int Terminal_ui::field_menu(string label, string key, string init,
            int x, int y, int w1, int w2, vector<string> &options)
{
    Field_entry *field_entry = new Field_entry(label, key, init, 0, 0,
                                               x, y , w1, w2, FIELD_MENU);
    field_entry->options = &options;
    fields.push_back(field_entry);
    return (int) fields.size() - 1;
}


void Terminal_ui::set_menu_options(string key, vector<string> &options, int n)
{
    int i = find_field(key);
    assert(i >= 0);
    fields[i]->options = &options;
    fields[i]->content = options[n];
}


void Terminal_ui::dialog_escape_keys(string keys)
{
    escape_keys = keys;
}


void Terminal_ui::set_field_callback(field_callback_t callback)
{
    field_callback = callback;
}


void Terminal_ui::set_key_callback(key_callback_t callback)
{
    key_callback = callback;
}


void Terminal_ui::body_clear()
{
    int x, y;
    getmaxyx(stdscr, y, x);
    y -= bottom_size();
    for (int ln = top_size(); ln < y; ln++) {
        move(ln, 0);
        clrtoeol();
    }
}


void Terminal_ui::dialog_refresh()
{
    // body_clear();  -- clearing erases additional text
    for (Field_entry *fd : fields) {
        fd->show_content(this);
    }
    
    // clear blank lines too: first do line after New Configuration
    int i = find_field("name");
    int y = fields[i]->y + 1;
    move(y, 0);
    clrtoeol();
    
    // do line below New MIDI In to O2:
    i = find_field("midi_in_new");
    y = fields[i]->y + 1;
    move(y, 0);
    clrtoeol();
    
    // do line above Input device:
    i = find_field("arco_in_id");
    y = fields[i]->y - 1;
    move(y, 0);
    clrtoeol();
}


void Terminal_ui::dialog_begin()
{
    assert(!dialog_under_construction);
    if (dialog_completed) {
        delete_all_fields();
        dialog_completed = false;
    }
    field_callback = nullptr;
    dialog_under_construction = true;
    body_clear();
}


void Terminal_ui::dialog_end(int key)
{
    assert(!dialog_under_construction && dialog_mode);
    dialog_mode = false;
    if (field_callback) {
        (*field_callback)("DIALOG_END", key);
    }
    screen_refresh();
}


void Terminal_ui::dialog_run()
{
    assert(dialog_under_construction && !dialog_completed);
    dialog_under_construction = false;
    dialog_completed = true;
    dialog_mode = true;
    direct_mode = false;
    dialog_refresh();
    // sets to first field if we have one
    set_current_field(fields.size() > 0 ? fields[0] : nullptr);
}


bool Terminal_ui::has_value(string key)
{
    assert(!dialog_under_construction && !dialog_mode && dialog_completed);
    int i = find_field(key);
    assert(i >= 0);
    return fields[i]->content.size() > 0;
}


int Terminal_ui::get_int(string key, int dflt)
{
    assert(!dialog_under_construction && !dialog_mode && dialog_completed);
    int i = find_field(key);
    assert(i >= 0);
    return (fields[i]->content.size() > 0 ? stoi(fields[i]->content) : dflt);
}


double Terminal_ui::get_double(string key, double dflt)
{
    assert(!dialog_under_construction && !dialog_mode && dialog_completed);
    int i = find_field(key);
    assert(i >= 0);
    return (fields[i]->content.size() > 0 ? stod(fields[i]->content) : dflt);
}


bool Terminal_ui::get_bool(string key, bool dflt)
{
    assert(!dialog_under_construction && !dialog_mode && dialog_completed);
    int i = find_field(key);
    assert(i >= 0);
    return fields[i]->content.size() > 0 ? fields[i]->content[0] == 'T' : dflt;
}


string Terminal_ui::get_string(string key, string dflt)
{
    assert(!dialog_under_construction && dialog_completed);
    int i = find_field(key);
    assert(i >= 0);
    return fields[i]->content;
}


string Terminal_ui::get_ip(string key, string dflt)
{
    return content_to_ip(get_string(key, dflt));
}


int Terminal_ui::get_menu_index(string key)
{
    assert(!dialog_under_construction && !dialog_mode && dialog_completed);
    int i = find_field(key);
    assert(i >= 0);
    return fields[i]->current_option();
}


void Terminal_ui::dialog_insert_line(int n)
{
    assert(dialog_completed);
    int x, y;
    getmaxyx(stdscr, y, x);
    for (Field_entry *fe : fields) {
        if (fe->y >= n) {
            fe->y++;
        }
    }
    // move all the lines (maybe fields will be redrawn, but audio device
    // info may not
    wsetscrreg(stdscr, n, y - (int) bottom_lines->size());
    scrollok(stdscr, true);
    wscrl(stdscr, -1);
}


void Terminal_ui::dialog_remove_line(int n)
{
    assert(dialog_completed);
    int readi = 0;
    int writei = 0;
    while (readi < fields.size()) {
        Field_entry *fe = fields[readi];
        if (fe->y == n) {
            delete fe;
        } else {
            fields[writei] = fields[readi];
            writei++;
        }
        if (fe->y > n) {
            fe->y--;
        }            
        readi++;
    }
    // shrink the size if elements were deleted:
    fields.erase(fields.begin() + writei, fields.end());
    // make current_field valid (assume it was removed). Assign to first field
    // in the same line. If none, find first field of previous line.
    int field_line = -1;
    current_field = NULL;
    for (Field_entry *fe : fields) {
        if (fe->y > field_line) {
            field_line = fe->y;
            current_field = fe;
        }
        if (fe->y == n) {
            return;
        }
    }
    return;  // maybe current_field will be NULL if no fields at all
}


// this moves a probably newly created field (but could be any field)
//
void Terminal_ui::dialog_move_field(int field_index, string location_key,
                                    bool before)
{
    Field_entry *field_to_move = fields[field_index];
    assert(field_to_move);
    int insert_index = find_field(location_key);
    if (insert_index == -1) {  // location_key has not yet been inserted
        assert(before);  // we are in fields, so will be before location_key
        return;
    }
    fields.erase(fields.begin() + field_index);  // move to new location
    // now every element above field_index has index decremented
    if (insert_index > field_index) {
        insert_index--;
    }
    // insert_index is where to insert *before* location_key, so adjust:
    if (!before) {
        insert_index++;
    }
    fields.insert(fields.begin() + insert_index, field_to_move);
}


void Terminal_ui::get_string(const char *prompt, string_callback_t callback)
{
    assert(!getting_string);  // not reentrant
    getting_string = true;
    string_callback = callback;
    int x, y;
    getmaxyx(stdscr, y, x);
    move(y - 4, 0);  // draw horizontal line to delineate input line
    hline(ACS_HLINE, 72);
    mvprintw(y - 3, 0, prompt);  // clear line for input
    printw(": ");
    clrtoeol();
}


/*
void Terminal_ui::config_int(string label, string key, int min, int max,
                    int x, int y, int w1, int w2)
{
    
}


void Terminal_ui::config_double(string label, string key, double min, double max,
                       int x, int y, int w1, int w2)
{
}


void Terminal_ui::config_string(string label, string key,
                       int x, int y, int w1, int w2)
{
}


void Terminal_ui::config_bool(string label, string key,
                     int x, int y, int w1, int w2)
{
}


void Terminal_ui::config_button(string label,
                       int x, int y, int w1, int w2)
{
}


void Terminal_ui::config_menu(string label, string key,
                     int x, int y, int w1, int w2, string *options)
{
}
*/

void Terminal_ui::add_help(string help_string)
{
    help_strings.push_back(help_string);
}


void Terminal_ui::clear_help()
{
    help_strings.clear();
    help_strings.push_back("p - scroll back one line");
    help_strings.push_back("b - scroll back one page");
    help_strings.push_back("n - scroll forward one line");
    help_strings.push_back("SPACE - scroll forward one page");
    // must be at index 4 (see help_keys()):
    help_strings.push_back(help_command_keys + " - print this help");
    help_strings.push_back("");
}


void Terminal_ui::help_keys(string keys)
{
    help_command_keys = keys;
    help_strings[4] = keys + " - print this help";
}


void Terminal_ui::poll(int delay_usec)
{
    fd_set s_rd, s_wr, s_ex;
    static int msgcnt = 0;
    struct timeval tv;

    int ch = getch();
    if (ch == ERR) {
        fflush(stdout);
        FD_ZERO(&s_rd);
        FD_ZERO(&s_wr);
        FD_ZERO(&s_ex);
        FD_SET(out_pipe[0], &s_rd);
        tv.tv_sec = 0;
        tv.tv_usec = delay_usec;
        int n = select(out_pipe[0] + 1, &s_rd, &s_wr, &s_ex, &tv);
        if (n <= 0) {
            return false;
        }
        char buffer[80];
        n = (int) read(out_pipe[0], buffer, 79);  // get, display input
        if (n > 0) {
            buffer[n] = 0;
            output(buffer);
        }
    } else {
        if (dialog_mode) {
            if (strchr(escape_keys.c_str(), ch) != nullptr) {
                dialog_end(ch);
            } else {
                dialog_handle_typing(ch);
            }
            return false;
        } else if (getting_string) {
            got_a_char(ch);
            addch(ch);
            return false;
        } else if (help_mode) {  // new input, so redraw screen, erase choices
            advance(0);
        }
        if (strchr(help_command_keys.c_str(), ch)) {
            show_help();
        } else {
            switch (ch) {
              case 'p':  // back one line
                advance(-1);
                break;
              case 'b':  // back one page
                advance(-2);
                break;  
              case 'n':  // forward one line
                advance(1);
                break;
              case ' ':  // forward one page
                advance(2);
                break;
              case KEY_RESIZE:
                resizeterm(0, 0);
                wclear(stdscr);
                screen_refresh();
                touchwin(stdscr);
                // redrawwin(stdscr);
                refresh();
                break;
              default:
                return (*key_callback)(ch, false);
            }
        }
    }
    return false;
}


// find first field with a matching key, return -1 if not found
int Terminal_ui::find_field(string key)
{
    int i = 0;
    for (Field_entry *fe : fields) {
        if (fe->key == key) {
            return i;
        }
        i++;
    }
    return -1;
}


void Terminal_ui::delete_all_fields()
{
    for (Field_entry *fe : fields) {
        delete fe;
    }
    fields.clear();
    current_field = NULL;
}


void Terminal_ui::set_current_field(Field_entry *field)
{
    current_field = field;
    if (field) {
        dialog_x = field->x + field->w1 + 1;
        if (current_field->field_type == FIELD_BOOL) {
            dialog_x++;  // fields looks like "[X]"
        }
        dialog_y = field->y;
        move(dialog_y, dialog_x);
        wrefresh(stdscr);
    }
}


void Terminal_ui::move_to_previous_field()
{
    assert(!dialog_under_construction && dialog_completed && dialog_mode);
    if (fields.size() == 0) {
        return;
    }
    for (int i = 0; i < fields.size(); i++) {
        if (fields[i] == current_field) {
            i--;
            if (i < 0) {
                i = (int) fields.size() - 1;
            }
            set_current_field(fields[i]);
            return;
        }
    } 
   set_current_field(fields[0]);
}


void Terminal_ui::tab_to_field()
{
    assert(!dialog_under_construction && dialog_completed && dialog_mode);
    assert(current_field != nullptr);
    int y_before = current_field->y;
    auto it = find(fields.begin(), fields.end(), current_field);
    assert(it != fields.end());
    it = next(it);
    if (it == fields.end()) {
        it = fields.begin();
    }
    set_current_field(*it);
}


void Terminal_ui::dialog_handle_typing(int ch)
{
    if (!current_field) {
        if (fields.size() == 0) {
            return;
        }
        set_current_field(fields[0]);
    }
    if (ch == '\t' || ch == '\n') {
        tab_to_field();
    } else if (ch == KEY_RIGHT) {
        if (!current_field->options &&
            current_field->cursor_in_field_text(dialog_x, dialog_y)) {
            move(dialog_y, ++dialog_x);
        } else {
            tab_to_field();
        }
    } else if (ch == KEY_LEFT) {
        if (!current_field->options &&
            current_field->cursor_after_field_text(dialog_x, dialog_y)) {
            move(dialog_y, --dialog_x);  // move left within typed content
        } else {
            move_to_previous_field();
        }
    } else if (!current_field->options && ch == KEY_DOWN) {
        move_to_line(1);
    } else if (!current_field->options && ch == KEY_UP) {
        move_to_line(-1);
    } else if (strchr(escape_keys.c_str(), ch)) {
        dialog_end(ch);
    } else {
        current_field->handle_typing(ch, this);
    }
}


void Terminal_ui::move_to_line(int direction)
{
    assert(!dialog_under_construction && dialog_completed && dialog_mode);
    if (fields.size() == 0) {
        return;
    }
    if (!current_field) {
        current_field = fields[0];
        return;
    }
    int y = current_field->y + direction;
    while (true) {  // if no other line has a field, this will wrap
                    // around to the first field in the current_field's line
        for (Field_entry *fe : fields) {
            if (fe->y == y) {
                set_current_field(fe);
                return;
            }
        }
        y = (y + direction + LINES) % LINES;
    }
}


// scroll the screen: 1 or -1 -> 1 line, 2 or -2 -> 1 page
//
void Terminal_ui::advance(int n)
{
    TUDBG(for (int i = 0; i < (int) lines.size(); i++) {
              fprintf(logfile, "%s\n", lines[i]);
          }
          fprintf(logfile, "Entering advance with lines just printed. n=%d\n", n);
          fflush(logfile);
    )

    int maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);
    int h = maxy - top_size() - bottom_size() - 1;  // number of scrolling lines,
                                 // leaving a space at the bottom for new type-in
    if (direct_mode) {
        display_index = (int) lines.size() - h;
    }
    // change n to one of -h, -1, +1, +h:
    if (n < -1) {
        n = -h;
    } else if (n > 1) {
        n = h;
    }
    
    display_index = display_index + n;
    // if display_index is near the end of output, set it back to display
    //    the last full screen (h) of lines:
    if (display_index + h > (int) lines.size()) {
        display_index = (int) lines.size() - h;
    }
    
    // if we are trying to display below the beginning of the output buffer,
    //    display from the beginning:
    if (display_index < 0) {
        display_index = 0;
    }

    // update display with n lines starting at display_index, where n
    // is the min of h and the number of lines. If we are not at the
    // end of the lines, we can use the last line for text (so height
    // is h + 1), otherwise we leave a blank line at the end for the cursor.
    move(top_size(), 0);
    n = ((int) lines.size() > h ? h : (int) lines.size());
    if (display_index + n < (int) lines.size()) {
        n = n + 1;  // we can use one more line of display
    }
    int i;
    int line_loc = top_size();
    for (i = 0; i < n; i++) {
        string *s = lines[display_index + i];
        move(line_loc, 0);
        clrtoeol();
        move(line_loc, 0);
        addstr(s->c_str());
        line_loc++;
    }
    out_line = line_loc;  // where to put direct_mode output (type-in)
    
    // are we in direct_mode? (Compute before we might change h)pp
    direct_mode = (display_index >= (int) lines.size() - h);
    help_mode = false;  // we erased whatever was there

    // erase anything left on the screen from a dialog or info
    h += top_size() + 1;
    while (line_loc < h) {
        move(line_loc, 0);
        clrtoeol();
        line_loc++;
    }
    
    if (direct_mode) {
        mvprintw(out_line, 0, curline.c_str());
    }
    move(out_line, out_col);  // restore cursor (if not in direct_mode,
        // this will put cursor below text
    
    TUDBG(
      fprintf(logfile, "advance: display_index %d lines.size() %d h %d "
              "direct_mode %d\n", display_index, (int) lines.size(),
              h, direct_mode);
      fprintf(logfile, "advance: out_line %d out_col %d n %d\n",
            out_line, out_col, n);
      fflush(logfile);
    refresh();)
}


static void print_extras()
{
    printf("(I)nfo - get IDs and info on audio devices\n");
}


void Terminal_ui::show_help()
{
    int x, y;
    getmaxyx(stdscr, y, x);
/*
    int n = 0;
    for (string &s : help_strings) {
        if ((*hs)[0] != 'p') n++;
    }
*/
    // navigation commands:
    int help_loc = y - bottom_size() - (int) help_strings.size();
    move(help_loc - 1, 0);
    hline(ACS_HLINE, 72);
    for (const string& str : help_strings) {
        mvprintw(help_loc, 0, str.c_str());
        clrtoeol();
        help_loc++;
    }
    move(y - 1, 0);
    refresh();
    help_mode = true;
/*
    int h = y - 5;  // number of scrolling lines
    if (direct_mode) {
        display_index = (int) lines.size() - h;
    }
*/
    direct_mode = false;
}


// we have a line of output in curline
// insert a copy of curline at the end of our queue of lines
//
void Terminal_ui::output_line()
{
    // do we need to make room at the end of lines?
    if ((int) lines.size() >= lines_max) {  // yes
        if (!direct_mode) {
            if (display_index == 0) {
                advance(1);  // before we lose the first line
            }
            display_index--;
        }
        delete lines[0];
        lines.erase(lines.begin());
    }
    lines.push_back(new string(curline));
    curline.clear();
    TUDBG(fprintf(logfile, "wrote %s to lines[%d]\n", s,
                  (int) lines.size() - 1);)
}

// we have a character to add to output. Characters are appended
// to curline and long lines are truncated.
//
void Terminal_ui::output_char(char c)
{
    if (curline.size() < 120) {
        curline += c;
    }
}


// in direct_mode, output a newline: scroll screen if needed
// and advance to next line
//
void Terminal_ui::newline(int maxx, int maxy)
{
    if (out_line >= maxy - 3) {  // need to scroll
        move(2, 0);           // move cursor to top left
        insdelln(-1);         // delete a line at the top
        move(maxy - 3, 0);    // insert a line at the bottom
        insdelln(1);
        move(maxy - 3, 0);    // move to the new empty line
        clrtoeol();           // erase what's there now
        out_line = maxy - 3;  // set the output location
    } else {  // otherwise, just move to the next (empty) line
        out_line++;
    }
    out_col = 0;
    move(out_line, out_col);  // leave the cursor on the new line
}


// buffer is a bunch of characters just written to stdout and
// read in via a pipe. This function is called to display them.
// All characters go into lines via output_char().
// In direct_mode, we can write the character directly to the
// screen as well.
//
void Terminal_ui::output(char *buffer)
{
    // first, copy to scrollable queue of output lines
    for (int i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '\n') {
            output_line();
        } else {
            output_char(buffer[i]);
        }
    }
    if (!direct_mode || dialog_mode) {
        return;
    }
    
    // in direct_mode and no dialog up, update the display
    int x, y;
    getmaxyx(stdscr, y, x);
    move(out_line, out_col);
    while (*buffer) {
        if (*buffer == '\n') {
            newline(x, y);
        } else {
            addch(*buffer);
            out_col++;
            if (out_col >= x) {
                newline(x, y);
            }
        }
        buffer++;
    }
    refresh();
}


int Terminal_ui::top_size()
{
    if (top_lines) {
        int n = (int) top_lines->size();
        return n > 0 ? n + 1 : 0;
    } else {
        return 0;
    }
}


int Terminal_ui::bottom_size()
{
    if (bottom_lines) {
        int n = (int) bottom_lines->size();
        return n > 0 ? n + 1 : 0;
    } else {
        return 0;
    }
}
 

void Terminal_ui::screen_refresh()
{
    if (dialog_mode) {
        dialog_end(KEY_RESIZE);
    }
    // put static info at bottom:
    if (bottom_size() > 0) {
        int x, y;
        getmaxyx(stdscr, y, x);
        // we never want lines to hold less than the screen size
        // (actually we only need the scrollable area to be
        // buffered in lines, but since the scrollable area depends
        // on changeable top_lines and bottom_lines, y is a
        // conservative number.
        if (y > lines_max) {
            lines_max = y;
        }
        // tricky: last line is at y-1, so y-bottom_size() is location
        // of the horizontal line where the first bottom_line text goes
        int i = y - bottom_size();  // location of text lines
        move(i++, 0);   // horizontal line just above text
        hline(ACS_HLINE, 72);
        for (string &s : *bottom_lines) {
            mvprintw(i++, 0, s.c_str());
            clrtoeol();
        }
    }

    move(0, 0);  // in case no top or bottom info

    // put static info at top:
    if (top_size() > 0) {
        int i = 0;
        for (string &s : *top_lines) {
            mvprintw(i++, 0, s.c_str());
            clrtoeol();
        }
        move(i++, 0);
        hline(ACS_HLINE, 72);
        move(i++, 0);  // leave cursor just below top info
    }
    advance(0);
}


const int BUFFER_LEN = 80;
static char buffer[BUFFER_LEN];
static int buffer_x = 0;


void Terminal_ui::got_a_char(int ch)
{
    if (ch == '\n') {
        buffer[buffer_x] = 0;
        (*string_callback)(buffer);
        getting_string = false;
        if (uiscr) {  // restore screen erased by prompt and input:
            advance(0);
        }
    }
    if (buffer_x < BUFFER_LEN - 1) {
        buffer[buffer_x++] = ch;
    }
}


