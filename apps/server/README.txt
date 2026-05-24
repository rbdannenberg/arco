This "app" illustrates how to build an Arco server to run in a terminal.

You can use this as is by connecting via O2 or O2lite.

You can customize this by changing dspmanifest.txt to select the unit
generators you want to have available.

You could also add commands to make sounds, respond to MIDI, or
control Arco in other ways directly from this application.

INSTALLATION
------------
On Windows, Arco uses native tools as opposed to MinGW/POSIX
tools. (Perhaps there should be another port to MinGW/POSIX, which is
really a different port.)

For Windows, you need pdcurses. Install with vcpkg:
    vcpkg install pdcurses:x64-windows

You also need form, which is usually part of curses or ncurses, but
not pdcurses. For Win32, the entire form library is in
arco/server/form_local which is compiled and linked to your
application automatically.
