# Arco Design Notes
Roger B. Dannenberg

## Preferences
Preferences specify:

- `*_in_id` -- PortAudio input device number
- `*_out_id` -- PortAudio output device number
- `*_in_name` -- PortAudio input device name (or substring thereof)
- `*_out_name` -- PortAudio output device name (or substring thereof)
- `*_in_chans` -- input channel count
- `*_out_chans` -- output channel count
- `*_buffer_size` -- samples per buffer
- `*_latency` -- output latency in ms (the output buffer should be
about this long, or if double-buffered, each buffer should be about
this long. The input buffer should also be this long, but we assume
it is empty when the output buffer is full, so input buffers do not
contribute additional latency.)

There are several sets of preference variables with different prefixes:
- `p_*` -- private to prefs module; values correspond to .arco prefs file
- `arco_*` -- Used for getting preferences from the user interactively.
- `actual_*` -- Parameters determined after devices were actually opened.
These values are displayed in interfaces as the "Current value" of any
parameter.

**General information flow:**
- `actual_*` values are provided by PortAudio when devices are opened.
They are used by the audio callback to interpret the audio stream.
- `p_*` values are initialized from the preferences file `.arco`
- `arco_*` values are initialized from `p_*` values, and when the user
sets valid `arco_*` values, they are used to update `p_*` values.
(Setting to -1 or some indication of "default" is also valid and
means "use default value instead of whatever might have been in the
preferences file", i.e., "clear the preference".)
- Values used to open PortAudio devices are the `p_*` values unless
they are -1 in which case default values are used.

Preferences are only written on request by writing the current `p_*` 
values, which can include -1 for "no preference".

**Preferences inside/outside the server**
Applications pass in parameters to open audio: device ids, channel counts, buffer size and latency. -1 works to get default values, but it is up
to applications to manage preferences.

Therefore preferences are on the "server side" and not visible to
the "arco side". To allow inspection of actual values selected, the
arco side sends actual values back to the control service, the name
of which is provided in /arco/open.

On the "arco side", we have only defaults and whatever values are
passed into the open operation.

**Synchronization and Threads**

Audio runs off a callback, so we should be careful with threads.
All communication is through O2, with preferences handled on the
main thread side. Actual values are sent from the audioio module
via O2 so either a shared-memory server or a remote application
can work the same way.

