/* iotest.cpp -- see if we can use cmtio in Xcode debugger
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include <stdlib.h>
#include <stdio.h>
#include "cmtio.h"


int main()
{
    IOsetup(0);
    while (true) {
        int c = IOgetchar();
        if (c != NOCHAR) {
            printf("%c\n", c);
        }
        if (c == 'q') {
            break;
        }
    }
    return 0;
}
