/* arco_ui.cpp -- implement a small UI and terminal output in one terminal
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

// Display contains lines starting at display_index
// When we hit lines_max lines, the first is freed, lines are shifted
// and a new line is added to the end. display_index is also shifted if
// not all lines are on the display, so that when you scroll back, the
// display does not update when new output arrives.
//
// However, display_index is never shifted below 0.  If new input keeps
// coming, eventually the screen will start scrolling again.
//
// There are 2 modes for scrolling: direct_mode is true if output is
// written immediately to the screen; direct_mode is false if the
// users scrolls back and new output is just captured in lines[].
// If not direct_mode, dialog_mode, or help_mode, input is used to
// scroll the text output.
//
// There's also dialog_mode, where scrolling stops and a form or help
// can be drawn.
//
// In dialog_mode, input is diverted to fieldentry_handle_typing unless
// help_mode, in which case any typing revert to direct_mode.
//
// To make a dialog (form), call ui_start_dialog(), create fields with
// ui_*_field(). While in dialog_mode, send input to
// fieldentry_handle_typing(). At end, call ui_end_dialog().

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <curses.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include "cmtio.h"
#include "o2internal.h"
#include "prefs.h"
#include "arco_ui.h"
#include "fieldentry.h"

Vec<O2string> arco_device_info;
bool arco_interrupt_requested = false;

#define ASCII_ESC 27

static FILE *ttyfd = NULL;
static FILE *logfile = NULL;  // for debugging
static SCREEN *uiscr = NULL;

static int out_pipe[2];
static int save_out;
static int save_err;

static int direct_mode = true;
static int help_mode = false;
static int dialog_mode = false;

static int lines_max = 0;
static char **lines = NULL;
static int lines_index = 0;  // index in lines of the next output line
static int display_index = 0;  // what line is top of display

static char curline[120];
static int curline_index = 0;

// cursor location when writing std output to screen
static int out_line = 2;
static int out_col = 0;

int stop = false;

void ui_end_dialog();


static void print_extras()
{
    printf("(I)nfo - get IDs and info on audio devices\n");
}


const char *help_strings2[] = {
    "p - scroll back one line",
    "b - scroll back one page",
    "n - scroll forward one line",
    "SPACE - scroll forward one page",
    NULL };


static void show_help()
{
    int x, y;
    getmaxyx(curscr, y, x);
    int n = 0;
    for (const char **hs = help_strings; *hs; hs++) {
        if ((*hs)[0] != 'p') n++;
    }
    move(y - 7 - n, 0);
    hline(ACS_HLINE, 72);
    // n is number of help strings
    int i = 0;
    for (const char **hs = help_strings; *hs; hs++) {
        if ((*hs)[0] != 'p') {
            mvprintw(y - 6 - n + i, 0, (*hs) + 1);
        }
        i++;
    }
    // navigation commands:
    for (int i = 0; i < 4; i++) {
        mvprintw(y - 6 + i, 0, help_strings2[i]);
    }
    refresh();
    help_mode = true;
    int h = y - 5;  // number of scrolling lines
    if (direct_mode) {
        display_index = lines_index - h;
    }
    direct_mode = false;
}


// scroll the screen: 1 or -1 -> 1 line, 2 or -2 -> 1 page
//
static void advance(int n)
{
    for (int i = 0; i < lines_index; i++) {
        fprintf(logfile, "%s\n", lines[i]);
    }
    fprintf(logfile, "Entering advance with lines just printed. n=%d\n", n);
    fflush(logfile);

    int maxx, maxy;
    getmaxyx(curscr, maxy, maxx);
    int h = maxy - 5;  // number of scrolling lines
    if (direct_mode) {
        display_index = lines_index - h;
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
    if (display_index + h > lines_index) {
        display_index = lines_index - h;
    }
    
    // if we are trying to display below the beginning of the output buffer,
    //    display from the beginning:
    if (display_index < 0) {
        display_index = 0;
    }

    // update display with n lines starting at display_index, where n
    // is the min of h and the number of lines. If we are not at the
    // end of the lines, we can use the last line for text (so height
    // is h + 1), otherwise we leave a blank line a the end for the cursor.
    move(2, 0);
    n = (lines_index > h ? h : lines_index);
    if (display_index + n < lines_index) {
        n = n + 1;  // we can use one more line of display
    }
    int i;
    for (i = 0; i < n; i++) {
        char *s = lines[display_index + i];
        move(i + 2, 0);
        clrtoeol();
        move(i + 2, 0);
        while (s && *s) {  // just clear lines if there is not text
            addch(*s++);
        }
        out_line = i + 3;
        out_col = 0;
        move(out_line, out_col);
    }
    // erase anything left on the screen from a dialog or info,
    // use h + 1 because there's always a blank line below the output
    while (i < h + 1) {
        move(i + 2, 0);
        clrtoeol();
        i++;
    }
    move(out_line, out_col);  // restore cursor
    
    // are we in direct_mode?
    direct_mode = (display_index >= lines_index - h);
    help_mode = false;  // we erased whatever was there
    fprintf(logfile, "advance: display_index %d lines_index %d h %d "
            "direct_mode %d\n", display_index, lines_index, h, direct_mode);
    fprintf(logfile, "advance: out_line %d out_col %d n %d\n",
            out_line, out_col, n);
    fflush(logfile);
    refresh();
}

// we have a line of output in curline up to curline_index
// insert a copy of curline at the end of our queue of lines
//
static void output_line()  // number of scrolling lines displayed
{
    curline[curline_index++] = 0;  // EOS
    char *s = (char *) malloc(curline_index);
    memcpy(s, curline, curline_index);
    // do we need to make room at the end of lines?
    if (lines_index >= lines_max) {  // yes
        if (!direct_mode) {
            if (display_index == 0) {
                advance(1);  // before we lose the first line
            }
            display_index--;
        }
        free(lines[0]);
        memmove(lines, lines + 1, sizeof(lines[0]) * (lines_max - 1));
        lines_index--;
    }
    lines[lines_index++] = s;
    curline_index = 0;
    fprintf(logfile, "wrote %s to lines[%d]\n", s, lines_index - 1);
}

// we have a character to add to output. Characters are appended
// to curline at curline_index and long lines are truncated.
//
static void output_char(char c)
{
    if (curline_index < sizeof(curline) - 1) {
        assert(curline_index < sizeof(curline));
        curline[curline_index++] = c;
    }
}


// in direct_mode, output a newline: scroll screen if needed
// and advance to next line
//
static void newline(int maxx, int maxy)
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
static void output(char *buffer)
{
    if (stop) return;

    // first, copy to scrollable queue of output lines
    for (int i = 0; buffer[i] != 0; i++) {
        if (buffer[i] == '\n') {
            output_line();
        } else {
            output_char(buffer[i]);
        }
    }
    if (!direct_mode) {
        return;
    }
    
    // in direct_mode, update the display
    int x, y;
    getmaxyx(curscr, y, x);
    move(out_line, out_col);
    while (*buffer) {
        if (*buffer == '\n') {
            newline(x, y);
        } else {
            addch(*buffer);
            out_col++;
            if (out_col >= x || out_col >= (sizeof(curline) - 1)) {
                newline(x, y);
            }
        }
        buffer++;
    }
    refresh();
}


static void refresh_screen()
{
    mvprintw(0, 0, "Arco v4");
    clrtoeol();
    mvprintw(0, 44, "Load:  0%%   Status: Stopped");
    move(1, 0);
    hline(ACS_HLINE, 72);
    int x, y;
    getmaxyx(curscr, y, x);
    move(y - 2, 0);
    whline(stdscr, ACS_HLINE, 72);
    mvprintw(y - 1, 0, main_commands);
    move(2, 0);
    nodelay(stdscr, TRUE);
    noecho();
    advance(0);
}


int ui_init(int count)  // count is how many output lines to save
{
    logfile = fopen("log.txt", "w");
    ttyfd = fopen("/dev/tty", "r+");
    if (!ttyfd) {
        return -1;  // fail
    }
    uiscr = newterm(NULL, ttyfd, ttyfd);
    set_term(uiscr);
    keypad(stdscr, true);
    save_out = dup(fileno(stdout));
    save_err = dup(fileno(stderr));
    pipe(out_pipe);

    dup2(out_pipe[1], fileno(stdout));
    dup2(out_pipe[1], fileno(stderr));

    lines = (char **)malloc(sizeof(lines[0]) * count);
    memset(lines, 0, sizeof(lines[0]) * count);
    lines_max = count;

    refresh_screen();
    return 0;
}

const int BUFFER_LEN = 80;
static char buffer[BUFFER_LEN];
static int buffer_x = 0;
static bool getting_string = false;
static void (*got_string_fn)(const char *s);
// if not in curses, you enter commands followed by newline, and if there
// are arguments, you want to skip the newline, prompt, and read a line
// up to the *next* newline. This variable tells us how many newlines to skip:
static int skip_newlines = 0;


void ui_get_string(const char *prompt, void (*callback)(const char *s))
{
    assert(!getting_string);  // not reentrant
    buffer_x = 0;
    getting_string = true;
    got_string_fn = callback;
    if (uiscr) {
        int x, y;
        getmaxyx(curscr, y, x);
        move(y - 4, 0);  // draw horizontal line to delineate input line
        hline(ACS_HLINE, 72);
        mvprintw(y - 3, 0, prompt);  // clear line for input
        printw(": ");
        clrtoeol();
        skip_newlines = 0;
    } else {
        skip_newlines = 1;
        printf("%s: ", prompt);
    }
}

static void got_a_char(int ch)
{
    if (ch == '\n') {
        if (skip_newlines--) {
            return;
        }
        buffer[buffer_x] = 0;
        (*got_string_fn)(buffer);
        getting_string = false;
        if (uiscr) {  // restore screen erased by prompt and input:
            advance(0);
        }
    }
    if (buffer_x < BUFFER_LEN - 1) {
        buffer[buffer_x++] = ch;
    }
}


int ui_poll(int delay_ms)
{
    /* DEBUGGING
    static int calls = 0;
    static int counter = 0;
    if (calls++ % (500 / delay_ms) == 0) {
        printf("counter: %d\n", counter++);  // 2Hz printing
    }
    DEBUGGING */
    int ch;
    if (!uiscr) {  // interface is just simple command line
        // note: for Xcode debugging curses, use Debug:Attach to Process
        usleep(delay_ms * 1000);
        ch = IOgetchar();
        if (ch <= 0) {
            return false;
        } else if (getting_string) {  // "modal" string input
            got_a_char(ch);
            return false;
        }
        switch (ch) {
          case 'H': { // help
            // n is number of help strings; skip last 4
            for (const char **hs = help_strings; *hs; hs++) {
                if ((*hs)[0] != 'c') {
                    printf("%s\n", (*hs) + 1);
                }
            }
            break;
          }
          case '\n':
            printf("%s\n", main_commands);
            break;
          default:
            return action(ch);
        }
        return false;
    }
    fd_set s_rd, s_wr, s_ex;
    static int msgcnt = 0;
    struct timeval tv;
    if (arco_interrupt_requested && dialog_mode) {  // fake an ESC to exit mode
        ch = ASCII_ESC;
    } else {
        ch = getch();
    }
    if (ch == ERR) {
        fflush(stdout);
        FD_ZERO(&s_rd);
        FD_ZERO(&s_wr);
        FD_ZERO(&s_ex);
        FD_SET(out_pipe[0], &s_rd);
        tv.tv_sec = 0;
        tv.tv_usec = delay_ms * 1000;
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
            if (ch == ASCII_ESC || ch == 'C' || ch == 'Q') {
                ui_end_dialog();
                if (ch == 'C') {
                    configure_screen_finish();
                } else if (ch == 'Q') {
                    action(ch);  // pass Q(uit) to main application...
                }
            } else {
                fieldentry_handle_typing(ch);
            }
            return false;
        } else if (getting_string) {
            got_a_char(ch);
            addch(ch);
            return false;
        } else if (help_mode) {  // new input, so redraw screen, erase choices
            advance(0);
        }
        switch (ch) {
          case 'H':  // help
            show_help();
            break;
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
          case 'e':  // write to stderr
            fprintf(stderr, "This is error output\n");
            fflush(stderr);
            break;
          case 'o':  // stdout
            printf("This is a test %d\n", msgcnt++);
            break;
          case 'r':  // refresh
          case KEY_RESIZE:
            refresh_screen();
            redrawwin(curscr);
            refresh();
            break;
          default:
            return action(ch);
        }
    }
    return false;
}


int ui_finish()
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
    for (int i = 0; i < lines_max; i++) {
        if (lines[i]) {
            free(lines[i]);
        }
    }
    free(lines);
    fclose(logfile);
    return false;
}


/************************* dialog manager **********************/

int field_top;

void ui_start_dialog(const char *title)
{
    mvprintw(0, 0, title);
    clrtoeol();
    move(1, 0);  // draw horizontal line to delineate input line
    hline(ACS_HLINE, 72);

    field_top = 2;  // line for first field
    dialog_mode = true;
    direct_mode = false;
}


// ui_int_field - make an editable field for integer input.
// prompt is a string label
// value is the value displayed and returned. -1 displays blank
// min and max are range of values
// actual and pref are dsiplayed as actual and pref value information
//
void ui_int_field(const char *prompt, int *value, int min, int max,
                  int actual, int pref, int id)
{
    // clear the line
    move(field_top, 0);
    clrtoeol();

    Field_entry *field = new Field_entry(0, (int) strlen(prompt) + 1, field_top,
                                 prompt, 6, FIELD_INT, id, value, nullptr);
    char value_string[32];
    if (*value != -1) {
        sprintf(value_string, "%d", *value);
        field->set_content(value_string);
    }
    field->show_content();

    char actual_str[32];
    if (actual == -1) {
        sprintf(actual_str, "<default>");
    } else {
        sprintf(actual_str, "%d", actual);
    }
    char pref_str[32];
    if (pref == -1) {
        sprintf(pref_str, "<default>");
    } else {
        sprintf(pref_str, "%d", pref);
    }
    mvprintw(field_top, (int) strlen(prompt) + 8, "Actual: %s, Pref: %s",
             actual_str, pref_str);
    field_top++;
}


// ui_bool_field - make an editable field for boolean input.
// prompt is a string label
// value is the value displayed and returned. -1 displays blank
// min and max are range of values
// actual and pref are dsiplayed as actual and pref value information
//
void ui_bool_field(const char *prompt, bool *value, bool actual,
                   bool pref, int id)
{
    // clear the line
    move(field_top, 0);
    clrtoeol();

    Field_entry *field = new Field_entry(0, (int) strlen(prompt) + 1, field_top,
                                prompt, 2, FIELD_BOOL, id, value, nullptr);
    char value_string[8];
    sprintf(value_string, "%s", *value ? "T" : "F");
    field->set_content(value_string);
    field->show_content();

    mvprintw(field_top, (int) strlen(prompt) + 8, "Actual: %s, Pref: %s",
             actual ? "T" : "F", pref ? "T" : "F");
    field_top++;
}


int imax(int a, int b) { return a ? a > b : b; }

void ui_menu_field(const char *prompt, char *value, const char **options,
                   const char *actual, const char *pref, int id)
{
    // clear the line
    move(field_top, 0);
    clrtoeol();

    int w = 0;
    for (const char **opt = options; *opt; opt++) {
        w = imax(w, (int) strlen(*opt));
    }
    Field_entry *field = new Field_entry(0, (int) strlen(prompt) + 1, field_top,
                                prompt, w, FIELD_MENU, id, value, nullptr);
    field->set_content(value);
    field->set_menu_options(options);
    field->show_content();

    mvprintw(field_top, (int) strlen(prompt) + w + 4, "Actual: %s, Pref: %s",
             actual, pref);
    field_top++;
}    


void ui_string_field(const char *prompt, char *value, int width,
                   const char *actual, const char *pref, int id)
{
    // clear the line
    move(field_top, 0);
    clrtoeol();

    Field_entry *field = new Field_entry(0, (int) strlen(prompt) + 1, field_top,
                  prompt, width, FIELD_STRING, id, value, nullptr);
    field->set_content(value);
    field->show_content();
    char actual_str[64];
    if (actual) {
        snprintf(actual_str, sizeof(actual_str), "Actual %s, ", actual);
    } else {
        actual_str[0] = 0;
    }
    mvprintw(field_top, (int) strlen(prompt) + width + 2,
             "%sPref: %s", actual_str, pref);
    field_top++;
}    


// call this after setting up dialog (this does not actually do
// character input -- pass characters to fieldentry_handle_typing())
void ui_run_dialog()
{
    set_current_field(nullptr);  // sets to first field
}


// ui_end_dialog() - finish dialog -- use the data from the form if good is true
//
void ui_end_dialog()
{
    for (Field_entry *field = fields; field; field = field->next) {
        const char *buffer = field->content;
        // convert blank input for numbers to -1 
        // (which means ignore and use the default):
        char trimmed[64];
        strncpy(trimmed, buffer, 64);
        trimmed[63] = 0;
        // trim trailing whitespace:
        char *ptr = trimmed + strlen(trimmed);
        while (ptr > trimmed && isspace(ptr[-1])) {
            *(--ptr) = 0;  // remove last whitespace character
        }
        // trim leading whitespace by advancing to first non-whitespace:
        ptr = trimmed;
        while (*ptr && isspace(*ptr)) ptr++;
        buffer = ptr;

        // empty string is translated to -1 if it is a number
        if (field->is_int() && *buffer == 0) {
            buffer = "-1";
        }
        if (field->is_int()) {
            const char *ptr = buffer;
            if (*ptr == '-') ptr++;
            if (!isdigit(*ptr)) continue;  // not digits, skip entry
            while (*ptr && isdigit(*ptr)) ptr++;
            if (*ptr) continue;  // we found a non-digit, skip entry
            *((int *) (field->value)) = atoi(buffer);
/* only 'i' and 'b' are currently implemented.
                } else if (fi->type == 'f') {
                    *fi->varptr.floatptr = atof(buffer);
                } else if (fi->type == 's') {
                    strcpy(fi->varptr.charptr, buffer);
 */
        } else if (field->is_bool()) {
            *((bool *) (field->value)) = (*buffer == 'T');
        } else if (field->is_string() || field->is_menu()) {
            strcpy((char *) (field->value), buffer);
        } else {  // unhandled field type
            assert(false);
        }
    }
    dialog_mode = false;
    direct_mode = true;
    refresh_screen();
    delete_all_fields();
}


void do_command(Field_entry *field)
{
    printf("do_command %d\n", field->id);
}
