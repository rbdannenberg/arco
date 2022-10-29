/* svprefs -- implementation of preferences for server
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

/* In this implementation, preferences are stored in a file .arco in
   the current directory. Preferences are kept in shared variables
   (see prefs.{cpp,h}) for access by audioio or other modules.

   svprefs.{cpp,h} is separated from prefs.{cpp,h} so that a 
   different, menu-based implementation can be offered when there
   is a more elaborate GUI.
 */

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "prefs.h"
#include "svprefs.h"

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

void prefs_read()
{
    printf("prefs_read called\n");
    FILE *pf = fopen(".arco", "r");
    if (!pf) {
        return;
    }
    char line[80];
    char device[80];
    int n;
    char *line_ptr = line;
    size_t line_len = 80;
    while (getline(&line_ptr, &line_len, pf) > 0) {
        if (strstr(line_ptr, "audio_in_name:") != 0) {
            get_name(line_ptr + 14, device);
            prefs_set_in_name(device);
        } else if (strstr(line_ptr, "audio_out_name:") != 0) {
            get_name(line_ptr + 15, device);
            prefs_set_out_name(device);
        } else if (get_number(line_ptr, "in_chans:", &n)) {
            prefs_set_in_chans(n);
        } else if (get_number(line_ptr, "out_chans:", &n)) {
            prefs_set_out_chans(n);
        } else if (get_number(line_ptr, "buffer_size:", &n)) {
            prefs_set_buffer_size(n);
        } else if (get_number(line_ptr, "latency:", &n)) {
            prefs_set_latency(n);
        }
    }
    if (line_ptr != line) {
        free(line_ptr);
    }
}


void prefs_write()
{
    FILE *pf = fopen(".arco", "w");
    if (!pf) return;
    fprintf(pf, "audio_in_name: \"%s\"\n", prefs_in_name());
    fprintf(pf, "audio_out_name: \"%s\"\n", prefs_out_name());
    fprintf(pf, "in_chans: %d\n", prefs_in_chans());
    fprintf(pf, "out_chans: %d\n", prefs_out_chans());
    fprintf(pf, "buffer_size: %d\n", prefs_buffer_size());
    fprintf(pf, "latency: %d\n", prefs_latency_ms());
    fclose(pf);
    printf("audio_in_name: \"%s\"\n", prefs_in_name());
    printf("audio_out_name: \"%s\"\n", prefs_out_name());
    printf("in_chans: %d\n", prefs_in_chans());
    printf("out_chans: %d\n", prefs_out_chans());
    printf("buffer_size: %d\n", prefs_buffer_size());
    printf("latency: %d\n", prefs_latency_ms());
}



