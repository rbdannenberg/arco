# server.md - information about the command-line server

Arco can be run as a library or as a process. The server project
creates a server that runs in a terminal and uses Curses to implement
a simple user interface for selecting audio devices and some other
basic functions.

## Preferences
Preferences within Arco source code are confusing enough that you
will need this to understand the code:

There are several sets of preference variables with different prefixes:
- `p_*` -- private to prefs module; values correspond to .arco prefs file
- `arco_*` -- Used for getting preferences from the user interactively.
- `host_*` -- Parameters determined after devices were actually opened.
  These values are displayed in interfaces as the "Actual" value of any
  parameter.

These prefixes are applied to each of the following suffixes. Here is
what they mean:

- `*_in_id` -- PortAudio input device number
- `*_out_id` -- PortAudio output device number
- `*_in_name` -- PortAudio input device name (or substring thereof)
- `*_out_name` -- PortAudio output device name (or substring thereof)
- `*_in_chans` -- input channel count
- `*_out_chans` -- output channel count
- `*_buffer_size` -- samples per audio buffer (not necessarily the
  same as the Arco block length, BL, as Arco may need to compute
  multiple blocks to fill the buffer on each audio callback).
- `*_latency` -- output latency in ms (the output buffer should be
  about this long, or if double-buffered, each buffer should be about
  this long. The input buffer should also be this long, but we assume
  it is empty when the output buffer is full, so input buffers do not
  contribute additional latency.)
- `*_network_enable` -- is networking enabled? If not, only
  connections within the local machine are possible.
- `*_o2lite_enable` -- is o2lite enabled? Implies `network_enable`.
- `*_internet_enable` -- is internet enabled? If not, O2 will not
  wait to determine a public IP address, causing delays if there is
  no internet access.
- `*_mqtt_enable` -- is MQTT enabled? If not, O2 cannot establish
  wide-area connections and only uses Bonjour.
  
### General preference information flow:**
- `host_*` values are provided by PortAudio when devices are opened.
  The `host_network_enable`, `host_o2lite_enable`,
  `host_internet_enable`, and `host_mqtt_enable` are not related to
  PortAudio. Instead, they are set and applied at server startup and
  cannot be changed without restarting.
  
The other `host_*` values are used by the audio callback to interpret
the audio stream.
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

Initially, arco_* values are set to preferences. They can be changed.
Changes are *not* automatically saved back to the .arco preference file.
You must use the 'P' command to save them.

Default in and out channels is 2. 

### Preferences inside/outside the server**
Applications pass in parameters to open audio: device ids, channel
counts, buffer size and latency. -1 works to get default values.

Preferences are on the "server side" and not visible to
the "arco side". To allow inspection of actual values selected, the
arco side sends actual (`host_*`) values back to the control service,
the name of which is provided in /arco/ctrl.

On the "arco side", we have only defaults and whatever values are
passed into the open operation.

## State Transitions

There are three parties: Arco thread, Host process, Client process.
All participate in audio state transitions, since audio can be
started and stopped manually by the Host and also by the Client.

### State Transitions in Arco Audio Service

Initial state is IDLE.

State changes are driven within the Host by server_goal_state, which
is the desired state, which cannot always be reached atomically.

There are 2 ways to get audio running:

- Typing "S" on the console starts (or stops) audio.

- Sending /host/run with parameter 1 (non-zero).

Either method sets server_goal_state to RUNNING, so eventually,
audio should start, setting the state to RUNNING.

Once running, audio can be stopped:

- Typing "S" on the console stops audio if it is running by
  setting the server_goal_state to IDLE.
  
- Sending /host/run with parameter 0

- Sending /host/clear sets server_goal_state to RESET_IDLE
  which stops audio and deletes all unit generators.
  
In the first case (console control), audio can be resumed
by typing "S" again.

In the second case (client message), the host will receive
/host/stopped to confirm when the audio is stopped. Then,
(with server_goal_state == RESET_IDLE), /arco/reset is sent
to free all ugens. Then, /host/reset is sent to notify the
host. The host then sends /actl/reset to the client, which
can then re-initialize Arco.

To exit the server, the user can type "Q", which sets
server_goal_state to FINISHED. This will close audio,
clean up o2 and Arco's file IO thread and exit the server.
There is no (current) message interface for the client
to shut down the server, and the client should NOT send
to /arco/quit, which will cause the audio thread to
exit (permanently), but will not shut down the server.

## host vs actl Services

Arco uses ctrl_service_addr to hold the service name for
arco reply and notification messages. It is "actl" by default.

In some cases, "actl" an entire callback address is a
parameter, e.g., with /arco/devinf, but most cases use
a service name set using /arco/ctrl.

The host expects to receive from Arco:

- /host/reset (audio ugens are freed)

- /host/starting (tells host about actual selected audio
  devices)

- /host/started (audio callbacks are happening)

- /host/stopped (reply to /arco/close)

These messages are sent to host_service_addr which is
initially "host" but can be set using /arco/host.

## Server States and Client Control

During initialization, client uses arco_engine.py to
set up connections and configure the pyarco module.
    arco.initialize(o2_debug_flags="rs")
will configure everything and return when audio is
running. This is a blocking call that has a timeout.
See the code for additional parameters.

After initialization, audio will be running. Client should call:
    readyfn()
which is client-defined to create an initial patch and start
controlling Arco.

Audio can be started or stopped with:
    `arco.start_audio(run_stop)`
where `run_stop` is True (run) or False (stop).

A command *could* reset the server and start clean. To do this,
    arco.reset(readyfn)
where readfn is the same function used after initialization, or
a new function to call after Arco has been reset. The function
will be called automatically but not immediately. arco.reset()
is non-blocking and it takes time to reset the Arco server.
The reset method automatically creates these unit generators
before calling readyfn:
    arco.zero, arco.zerob, arco.input, arco.output

## Scheduling and Time

When arco.initialize returns or arco.reset calls readyfn,
the scheduler is (re)initialized. Any pending events are
cleared. The default real-time scheduler, sched.rtsched
is initialized so that `sched.get_time()` gets its time from
`o2lite.get_time()`, so the scheduler current logical time,
sched.rtime, is based on time from o2lite.

The virtual scheduler, however, has an initial logical time
of zero, so `sched.vtime` is initially zero. Since `sched.cause`
time offsets are relative to logical time, the absolute
values of various times should not matter, but if you need to
get a timestamp, it is convenient that `o2lite.get_time()` (which
is also O2's global time), `sched.rtime`, and `sched.get_time`
are all compatible and based on the same O2 time.)
