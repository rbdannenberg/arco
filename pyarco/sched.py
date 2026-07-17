# sched.py -- a scheduler framework
#
# Logical Time Theory
#    Scheduling associates an ideal time with every event (function call,
#      method invocation). The event is run as close to the logical
#      time as possible. Think of logical time is an idealized time or
#      specification. This section is mainly about where this
#      specification comes from. But before getting to that, we'll
#      describe the action of the scheduler: Logical times are mapped
#      to real time, and as soon as the actual real time reaches the
#      mapped logical time, the event is run. To map from logical time
#      to real time, Vscheduler's use a Time_map object to map to their
#      parent's time (Vscheduler's can form a tree with a Scheduler at
#      the root.) Scheduler's add time_offset to get from logical time
#      to real time.
#    Computing Logical Times. Logical times are normally relative to
#      another logical time. When you call cause(delta, ...), a new
#      event is created with the logical time of time + delta, where
#      time is the (ideal) logical time of the current event. If you
#      cause an event, and that event causes another event, and so on,
#      the logical times are the precise accumulation of all the
#      delta's and therefore there is no drift or quantization due to
#      execution time or finite polling rates. But what about the
#      first event in such a sequence?
#    IMPORTANT! Logical Time of the First Event in a Sequence.
#      When some unscheduled execution, e.g., from a user interface or the
#      network, needs to schedule an event, you must call sched.select(s),
#      which updates a scheduler as if we were supposed to be running an
#      event scheduled by s the at the current time. Then, cause(delta)
#      will schedule an event on scheduler s at offset delta from the
#      current time, using real or virtual time units depending
#      on the selected scheduler. Note that while this is happening,
#      time passes, so even if you schedule an event with cause(0,
#      ...), it might run *after* some other previously scheduled
#      event whose logical time is already in the past. ALTERNATIVELY,
#      you can write
#           sched.cause(absolute(abs_time), obj, meth, ...)
#      to give an absolute time reference (real or virtual) to the
#      scheduled event, but you still need to first call
#      sched.select(s) to designate the scheduler to be
#      used. sched.select() is only necessary before the first event
#      is scheduled. If a scheduled event (function or method) calls
#      cause(), then the delta passed to cause is relative to the
#      ideal event time of the current event, and the same scheduler
#      is used.
#
# Globals:
#    You should not import global variables, since import is copy-by-value.
#    Instead,
#        import sched
#        from sched import (rtsched, vtsched, absolute, real_delay)
#    Importing object references and functions is ok (except do not
#        from sched import time_get() -- WRONG
#    Instead, call sched.time_get() because time_get() can be replaced.)
#    Variables can be accessed as sched.current, sched.vtime, etc.
#
#    sched.rtsched - the single real-time scheduler object
#    sched.vtsched - an initial virtual-time scheduler object (more can be created)
#    sched.vtime -- the logical time of the current scheduler
#    sched.rtime -- the current ideal time of the real-time scheduler
#    sched.ctime -- the most recently computed time from sched.time_get(),
#            which is either real time from time_get() or
#            external clock time from sched.time_get(). This is
#            updated each time sched.poll() is called, so it should be
#            up-to-date except for polling period and possible latency
#            due to long computations. Using sched.ctime is faster than
#            calling sched.time_get().
#    sched.current - tells what scheduler dispatched the currently running
#           event (can be changed by a call to sched.select())
#    sched.running - set this to false to exit sched.run loop
#    sched.max_latency - the maximum time by which a scheduled call is late
#            for diagnosing the cause of unexpected delays or glitches
#    sched.nesting - nesting level of rtsched
#    sched.time_get() - a function to get the time; you can assign a custom
#            function to sched.time_get to change it
#    EPSILON - a small value, larger than rounding error
#    INFINITY - a time that will never be reached
#    sched.allow_late - Normally, the scheduler will exit with an error if
#            you attempt to schedule something in the past by more than
#            EPSILON. If sched.allow_late is set to True and a scheduled
#            time is in the past, a warning is printed, but the program
#            will not exit.

import time as _time
from bisect import insort
from timemap import Time_map



# --- Constants ---

EPSILON = 0.000001
M_EPSILON = -EPSILON
STOPPED_BPS = EPSILON / 60  # one beat every million minutes (1.9 years)
INFINITY = 1000000.0        # many hours
DEFAULT_POLL_PERIOD_MS = 2
SCHED_NO_PARMS = []

# --- Globals ---

trace = None
current = None

rtime = 0.0
vtime = 0.0
ctime = 0.0
o2_enabled = None
poll_functions = []

initialized = None    # detects if we've started before
nesting = 0           # non-zero when running scheduled events
nesting_error_printed = True
allow_late = False
running = False
max_latency = 0

rtsched = None
vtsched = None

midi_in = None

# Callback timing stats (disabled by default — see poll)
last_callback_time = 0
callback_stats_epoch_end = 0
callback_stats_epoch_dur = 10  # seconds
callback_max_period = 0
smallest_time_advance = 9999

_time_start = 0


# --- Time helpers (stubs; replace with real implementations) ---

def time_get():
    """Return current real time in seconds."""
    return _time.monotonic() - _time_start


def time_start():
    """Start the timer subsystem (stub)."""
    global _time_start
    _time_start = _time.monotonic()


def time_sleep(seconds):
    """Sleep for the given number of seconds."""
    _time.sleep(seconds)


def within(a, b, tol):
    """Return True if a and b are within tol of each other."""
    return abs(a - b) <= tol


def sendapply(obj, method_symbol, args):
    """
    Serpent: sendapply(obj, method, args) invokes the method named by
    method_symbol on obj, passing the argument list args.
    """
    fn = getattr(obj, method_symbol)
    return fn(*args)


# --- Module-level API helpers ---

def absolute(x):
    """Compute delta for cause() corresponding to absolute logical time x."""
    return x - current.time


def real_delay(delta):
    """
    Compute delta for cause() relative to real time, not logical time.
    Example: to schedule an event 0.1s from the current real time
    (so delays accumulate), call cause(real_delay(0.1), ...).
    """
    return absolute(current.r2v(time_get() + delta))


# --- Sched API functions ---

def init():
    """Prepare to run: create schedulers and prepare for real-time processing."""
    global rtsched, vtsched, initialized
    global max_latency, current
    if initialized:
        # do a reset on existing objects
        rtsched.init()
        vtsched.init(rtsched)
    else:
        rtsched = Scheduler()
        vtsched = Vscheduler(rtsched)
    time_start()  # now time_get() returns elapsed time from init()
    # print("sched.init setting rtsched.time_offset to", time_get())
    rtsched.time_offset = time_get()
    max_latency = 0
    initialized = True
    current = rtsched


def run(poll_period_ms=DEFAULT_POLL_PERIOD_MS, origin_to_now=False):
    """Run the scheduler loop until sched.running is set to False.
    
    You can "prime the pump" by scheduling some events
    relative to the time you call run(). Some time might
    elapse during initialization, so events scheduled for
    time zero might now be late. You can shift the time
    line so that the origin (zero) is "now" (the time of
    this function call) by passing origin_to_now=True.

    In other cases, if you want scheduler time synchronized
    to sched.get_time(), leave origin_to_now=False. You
    also need to override the initialization of this module
    and its schedulers, which will set the time origin to
    the time when the module is intialized.
    """
    global running
    running = True
    # print("sched.run setting rtsched.time_offset to", time_get())
    if origin_to_now:
        rtsched.time_offset = time_get()
    while running:
        # print("sched.run polling at", time_get())
        poll()
        time_sleep(poll_period_ms * 0.001)  # delay 2ms to allow other processes


def stop():
    """Tell run() to stop and return."""
    global running
    running = False


def time_get():
    """
    Return the current time. Always call using sched.time_get().
    You can assign a new time reference function to sched.time_get.
    """
    global ctime
    ctime = _time.monotonic()
    return ctime


def poll_function_add(fn):
    """Add fn (a callable) to the list of functions called on every poll."""
    if fn not in poll_functions:
        poll_functions.append(fn)


def poll():
    """Run scheduling. Can be handled by run()."""

    global last_callback_time, callback_max_period
    global callback_stats_epoch_end, smallest_time_advance

    rtsched.poll(time_get())
    # To avoid *always* importing o2 (or o2lite), you *must* call
    # poll_function_add(o2_poll) with some polling function for O2:
    for fn in poll_functions:
        fn()

    return
    # --- timing analysis below (unreachable by default;
    #     remove `return` to enable) ---
    period = ctime - last_callback_time
    callback_max_period = max(period, callback_max_period)
    if ctime > callback_stats_epoch_end:
        print("* longest recent callback period:",
              int(callback_max_period * 1000), "ms")
        if smallest_time_advance != 9999:
            print("* smallest time advance:", smallest_time_advance, "ms")
            smallest_time_advance = 9999
        callback_max_period = 0
        callback_stats_epoch_end = ctime + callback_stats_epoch_dur
    last_callback_time = ctime
    current.time = None  # no cause until selected again


def select(s):
    """Select s as the current scheduler and compute the current logical time."""
    s.select()


def cause(when, target, message, *parms):
    """Schedule an event on the current scheduler."""
    current.cause(when, target, message, list(parms))


def r2v(r):
    return current.r2v(r)


def v2r(v):
    return current.v2r(v)


def bump_time(d):
    current.bump_time(d)


def set_bpm(bpm):
    current.set_bpm(bpm)


def set_bps(bps):
    current.set_bps(bps)


def set_period(p):
    current.set_period(p)


def set_time(v):
    current.set_vtime(v)


def wait(period):
    current.wait_until(vtime + period)


def wait_until(when):
    current.wait_until(when)


def flush():
    current.flush()


# --- general_apply ---

def general_apply(target, message, parms):
    """
    Apply a function or method to parms.
    If target is None, message is a function. Call it.
    If target is an object, invokes the named method on it.
    parms must be a list.
    """
    if not isinstance(parms, list):
        raise TypeError(
            f"Expected parms in general_apply to be a list of\n"
            f"parameters to pass to {message!r}.  Perhaps you\n"
            f"called rtsched.cause() or vtsched.cause() with parameter\n"
            f"{parms!r} instead of calling select(rtsched) or\n"
            f"select(vtsched) and then\n"
            f"cause(..., {message!r}, {parms!r}).\n"
            f"Note that sched.cause() takes a list of 0 or more\n"
            f"parameters, while the cause() methods of Scheduler\n"
            f"(e.g. rtsched) and Vscheduler (e.g. vtsched) take a\n"
            f"list of parameters.  Using sched.cause() is preferred."
        )
    if target is not None:
        sendapply(target, message, parms)
    else:
        message(*parms)


# --- Scheduler class ---

class Scheduler:
    """Real-time scheduler. Events are stored in a time-sorted list."""

    def __init__(self):
        self.time = None
        self.queue = None
        self.next_time = None
        self.time_offset = None
        self.init()

    def init(self):
        self.time = 0.0         # ideal real time
        self.next_time = INFINITY
        self.queue = []


    def cause(self, when, target, message, parms, late_ok=False):
        """
        Schedule an event.
          when    - time offset from now until the event
          target  - object receiving message, or None for a plain function
          message - string (symbol) naming the method or function
          parms   - list of parameters
        """
        if when < M_EPSILON and not late_ok:
            print("WARNING: schedule at negative time:", when)
            if not allow_late:
                raise RuntimeError("ERROR: schedule at negative time: " +
                                   str(when))
        when = self.time + when  # convert offset to absolute time
        # using -e[0] for key to get sorting in decreasing order:
        insort(self.queue, [when, target, message, parms], key=lambda e: -e[0])
        if self.queue:
            self.next_time = self.queue[-1][0]
        else:
            self.next_time = INFINITY

    def flush(self):
        self.queue.clear()
        self.next_time = INFINITY

    def poll(self, when):
        """
        Must be called periodically. `when` is the current real time
        (from time_get()).
        """
        global nesting, nesting_error_printed, max_latency
        global rtime, vtime, current
        if nesting > 0:
            if not nesting_error_printed:
                nesting_error_printed = True
                print("#### WARNING: Recursive call to Scheduler::poll()!")
                print("####          Returning from poll().")
                # traceback.print_stack()
            return
        nesting_error_printed = False
        nesting = 1
        # DANGER: bump_time can change time_offset, so recompute each iteration
        # print("sched poll, now", when - self.time_offset, "next", self.next_time)
        while when - self.time_offset >= self.next_time - EPSILON:
            event = self.queue.pop()  # unappend (last = earliest)
            if when > 0:
                max_latency = max(max_latency,
                                  when - self.time_offset - self.next_time)
            if self.queue:
                self.next_time = self.queue[-1][0]
            else:
                self.next_time = INFINITY
            self.time = event[0]
            rtime = self.time
            vtime = self.time
            if trace:
                print(f"dispatch at {when - self.time_offset} ideal {self.time}"
                      f" msg {event[2]!r} behind"
                      f" {time_get() - (rtime + self.time_offset)}")
            current = self
            target = event[1]
            message = event[2]
            parms = event[3]
            general_apply(target, message, parms)
        nesting = 0
        # After all scheduled events, update time to reflect current real time
        self.time = when

    def get_tick(self):
        """
        Return an idealized integer millisecond count matching PortMidi
        timestamps, equal to int(time_get() * 1000).
        """
        if nesting == 0:
            raise RuntimeError("ERROR: Scheduler::get_tick() called, but "
                               "not in an event")
        delay = rtime + self.time_offset - time_get()
        return int((time_get() + delay) * 1000)

    def select(self):
        """Switch the current scheduler to this one."""
        global rtime, vtime, current
        if nesting == 0:
            self.time = time_get() - self.time_offset
            rtime = self.time
        # otherwise rtime was exactly set by the current event
        vtime = self.time
        current = self

    def r2v(self, r):
        """Map real time to virtual (local) time. Nested mapping stops here."""
        return r - self.time_offset

    def v2r(self, v):
        """Map virtual (local) time to real time."""
        return v + self.time_offset

    def bump_time(self, delta):
        """Advance scheduler time by delta (for time synchronization)."""
        self.time_offset -= delta
        self.time += delta

    def __str__(self):
        return self.to_string()

    def to_string(self, when=None):
        select(self)
        s = f"<Scheduler: time_offset {self.time_offset}, time {self.time}"
        if when is not None:
            s += f" maps {when} to {self.r2v(when)}"
        return s + ">"

    def show_queue(self):
        select(self)
        for ev in reversed(self.queue):
            print(f"    {ev[2]}@{ev[0]} with {ev[3]}")


# --- Vscheduler class ---


class Vscheduler(Time_map):
    """
    Virtual-time scheduler. Maps its own virtual time through a Time_map
    to its parent (real-time) scheduler's timeline.
    """

    def __init__(self, rts, name=""):
        super().__init__(bps=1.0)  # initial tempo: 60 bpm = 1 bps
        self.parent = None
        self.name = None
        self.time = None
        self.queue = None
        self.next_vtime = None
        self.adjust_num = None
        self.init(rts, name)

    def init(self, rts, name=""):
        self.parent = rts
        self.tminit(bps=1.0)  # initial tempo: 60 bpm = 1 bps
        self.name = name
        self.time = 0.0
        self.queue = []
        self.next_vtime = INFINITY
        self.adjust_num = 0

    def __str__(self):
        return self.to_string()

    def to_string(self, when=None):
        select(self)
        s = (f"<Vscheduler: rt {self.rt_base} vt {self.vt_base} bps {self.bps}")
        if when is not None:
            s += f" maps {when} to {self.r2v(when)}"
        return s + ">"


    def cause(self, when, target, message, parms, late_ok=False):
        """Schedule an event relative to this virtual time."""
        when = self.time + when
        insort(self.queue, [when, target, message, parms], key=lambda e: -e[0])
        if self.next_vtime > when:
            self.reschedule(late_ok)

    def flush(self):
        self.queue.clear()
        self.next_vtime = INFINITY

    def select(self):
        global current, vtime
        self.parent.select()       # recursively select from root
        current = self
        self.time = self.map_from_parent(self.parent.time)
        vtime = self.time

    def r2v(self, now):
        """Recursive mapping from real time to local virtual time."""
        return self.map_from_parent(self.parent.r2v(now))

    def v2r(self, vnow):
        """Recursive mapping from local virtual time to real time."""
        return self.parent.v2r(self.map_to_parent(vnow))

    def reschedule(self, late_ok=False):
        """Private: schedule a wake-up call for the next event in the queue."""
        if self.queue:
            self.next_vtime = self.queue[-1][0]
            self.parent.cause(
                self.map_to_parent(self.next_vtime) - self.parent.time,
                self, 'activate', SCHED_NO_PARMS, late_ok
            )
        else:
            self.next_vtime = INFINITY

    def activate(self):
        """
        Internal method called by parent scheduler to run the next event.
        Ignores early activations; dispatches one or more events when on time.
        """
        global vtime, current
        self.time = self.map_from_parent(self.parent.time)
        if self.time < self.next_vtime - EPSILON:
            return  # ignore early activate; another will come at the right time
        while self.time >= self.next_vtime - EPSILON:
            event = self.queue.pop()  # unappend (last = earliest)
            if self.queue:
                self.next_vtime = self.queue[-1][0]
            else:
                self.next_vtime = INFINITY
            self.time = event[0]  # should not be necessary
            vtime = self.time
            target = event[1]
            message = event[2]
            parms = event[3]
            current = self
            general_apply(target, message, parms)
        self.reschedule()  # in case no wake-up for next_vtime

    def set_vtime(self, v):
        """Set the scheduler time to v, updating the time map."""
        print("Vscheduler set_vtime", v)
        self.rt_base = self.parent.time
        self.vt_base = v
        if self.vt_base < -100:
            raise ValueError(f"vt_base={v} is too negative")
        self.time = v
        self.reschedule()

    def set_bpm(self, bpm):
        self.set_bps(bpm / 60)

    def set_bps(self, b):
        self.adjust_num += 1  # cancel any pending callback
        if b == 0:
            b = STOPPED_BPS
            print("WARNING: cannot set BPS to zero, using STOPPED_BPS instead")
        elif b < 0:
            print(f"ERROR: set_bps({self.bps}) ignored!")
            return
        self.rt_base = self.parent.time
        self.vt_base = self.time
        if self.vt_base < -100:
            raise ValueError(f"vt_base={self.vt_base} is too negative")
        self.bps = b
        self.reschedule(True)  # allow late due to rounding error

    def set_period(self, p):
        if p <= 0:
            print(f"ERROR: set_period({p}) ignored!")
        else:
            self.set_bps(1 / p)

    def set_time_map(self, rt, vt, bps_, late_ok=False):
        """Change the real-to-virtual mapping."""
        self.adjust_num += 1  # cancel any pending callback
        self.rt_base = rt
        self.vt_base = vt
        if self.vt_base < -100:
            raise ValueError(f"vt_base={vt} is too negative")
        self.bps = bps_
        self.reschedule(late_ok)

    def adjust_map(self, est_map, conv_dist, lag):
        """
        Smoothly interpolate from the current time map to est_map.
          est_map   - estimated Time_map mapping real time to output time
          conv_dist - convergence distance in beats (larger = smoother)
          lag       - latency in seconds
        """
        # adjust_map takes in a estimated time_map (e.g. from foot tapping) and
        # smoothly interpolates from this time map to the new one. It is assumed
        # that the parent is rtsched.
        #  est_map - the estimated time_map, mapping real time to OUTPUT time.
        #            est_map must NOT be modified until at least the next call
        #            to this adjust_map method.
        #  conv_dist - the convergence distance in beats, larger is smoother
        #  lag - latency. Assume this scheduler schedules everything early by
        #        lag because event OUTPUT happens lag seconds after events are
        #        scheduled. To make OUTPUT converge to est_map, we need to
        #        (eventually, after convergence) schedule at 
        #            (the time specified by est_map) - lag.
        # Behavior:
        #     Normally, change tempo so that we converge in conv_dist, but
        # reschedule adjust_map in 1/2 conv_dist so that we approach the 
        # target (est_map) exponentially. This has three problems:
        # 1. Speed to reach the target in conv_dist could be excessive.
        #      Therefore, limit the speed to between 1/2 and 2 times target bps.
        # 2. We never reach the target because of the negative exponential
        #      approach. When we get within 20 msec of the target and the speed
        #      is less than 2 times target bps, reschedule after conv_dist,
        #      so we fully converge. When we get within 2 msec of the target,
        #      then just set the actual map to the target and do not reschedule
        #      adjust_map.
        # 3. Fast convergence causes rapid recalculation of the tempo. E.g., if
        #      conv_dist is 0.1 sec, then we would update tempo every 50 msec
        #      (1/2 conv_dist). Instead of rescheduling every 1/2 conv_dist,
        #      reschedule after max(0.1, 0.5 * conv_dist), so at most 10 tempo
        #      updates per sec, and when conv_dist < 0.2, we converge more than
        #      1/2 way to the target before rescheduling, and when 
        #      conv_dist <= 0.1, we converge all the way to the target before
        #      rescheduling.
        select(self)
        conv_dist = max(0.1, conv_dist)
        diff = self.time - est_map.map_from_parent(self.parent.time + lag)

        self.adjust_num += 1  # cancel any pending adjust_map_n calls

        if (self.bps <= STOPPED_BPS or
                within(self.time,
                       est_map.map_from_parent(self.parent.time + lag), 0.002)):
            # Converged (or first-time): copy the estimated map, shifted by lag
            self.vt_base = est_map.vt_base
            self.rt_base = est_map.rt_base - lag
            self.bps = est_map.bps
            self.reschedule(True)  # late_ok: minor rounding issues can occur
        else:
            lagged_time = rtsched.time + lag
            self.vt_base = self.time
            self.rt_base = lagged_time
            # Compute bps so we intersect the target map after conv_dist beats
            # (Eq. 8 of the 2011 NIME paper)
            den = (est_map.rt_base * est_map.bps
                   - self.rt_base * est_map.bps
                   - est_map.vt_base
                   + self.vt_base
                   + conv_dist)
            if conv_dist < 2 * den:
                self.bps = (conv_dist * est_map.bps) / den
            else:  # fastest we allow; may take > conv_dist:
                self.bps = 2 * est_map.bps
            # Shift rt_base by latency so output matches desired timing
            self.rt_base -= lag
            self.next_vtime = INFINITY  # force reschedule in cause()
            self.cause(
                max(0.1, conv_dist / 2),
                self, 'adjust_map_n',
                [est_map, conv_dist, lag, self.adjust_num],
                True  # late_ok
            )

    def adjust_map_n(self, est_map, conv_dist, lag, n):
        """Private method used by adjust_map()."""
        if self.adjust_num == n:
            self.adjust_map(est_map, conv_dist, lag)

    def show_queue(self):
        """Print the pending events for debugging."""
        select(self)
        for i in range(len(self.queue) - 1, -1, -1):
            ev = self.queue[i]
            print(f"    {ev[2]}@{ev[0]}->{self.map_from_parent(ev[0])} "
                  f"with {ev[3]}")
        print(f"  Parent time {self.parent.time} Virt time {self.time}")
        print("  activate at parent times:")
        for i in range(len(self.parent.queue) - 1, -1, -1):
            ev = self.parent.queue[i]
            if ev[1] is self:
                print(f"    {ev[2]}@{ev[0]}->{self.map_from_parent(ev[0])} "
                      f"with {ev[3]}")
        print("  --------------------------")


# --- Periodic class ---
#
# To call a function periodically, with the ability to cancel and
# option to insure periodic activity is unique even if started
# more than once.
#
# Parameters:
#    per - the period in seconds or beats, running on the current scheduler
#    unique (optional) - name for the instance of Periodic
#    obj (optional) - the target object, or nil if meth is a function
#    meth (optional) - symbol for the targets's method or a global function
#    params (optional) - parameters passed to meth
#
# To start calling every 0.1 on current scheduler:
#    Periodic(0.1, nil, 'myfunc', p1, p2, ...).start() or
#    Periodic(0.1, myobject, 'mymethod', p1, p2, ...).start()
# calls myfunc or myobject.mymethod repeatedly with parameters p1, p2, ....
# To terminate, call the stop() method (you will need to save
#    a reference, to the object, e.g. 
#    myperiodic = Periodic(0.1, nil, 'myfunc', ...).start()
#    ...
#    myperiodic.stop()
# To (re)start after stopping, invoke start().  start() will automatically
# discontinue any previous scheduled activity of this instance as well as
# launch new scheduled periodic activity.
# The start() method takes an optional initial delay.
#
# Return values from myfunc or mymethod control the period:
#    return 'stop' to stop the sequence
#    return a number to set the period to that number
#    return nil to continue with the current period
#
# If debug is set to a string, print actions for debugging.
# The parameters (array) can be set to change parameters passed
# to meth.
#
# To insure a unique periodic activity, specify unique, e.g.
#    Periodic(0.1, nil, 'myfunc', p1).start
#    ... and later, this gets called again:
#    Periodic(0.1, nil, 'myfunc', p1).start
# will call 'myfunc' *twice* every 0.1 seconds; however,
#    Periodic(0.1, nil, 'myfunc', p1, unique = 'myfunc_polling').start
#    ... and later, this gets called again:
#    Periodic(0.1, nil, 'myfunc', p1, unique = 'myfunc_polling').start
# will call 'myfunc' *once* every 0.1 seconds. The second call will run
# 'myfunc' immediately, but the previous object will stop polling.
#
# Example: to run poll_for_conductor in the current object every 3 seconds
# using rtsched (time is in units of seconds):
#    cp = Periodic(3, this, 'poll_for_conductor', unique = 'conductor_poller')
#    cp.debug = "conductor_poller"  // requests trace output
#    sched_select(rtsched)  // make sure we're on the right scheduler
#    cp.start()
# If you do not set .debug, nothing will be printed.
#
# You can subclass Periodic, normally just overriding action(p1, p2, ...)
# which is passed any parameters. To construct the subclass with parameters,
# use Periodic_subclass(period, nil, nil, p1, p2, ...) and possibly include
# a symbol for keyword parameter unique. (The two parameters "nil, nil"
# are placeholders for target, and method, which are not used if you override
# active(). Your active() method should return nil, 'stop' or a new period
# just like method would.

class Periodic:
    """
    Call a function or method periodically, with the ability to cancel and
    an option to ensure periodic activity is unique even if started more
    than once.

    Parameters:
        per    - the period in seconds or beats, on the current scheduler
        obj    - the target object, or None if meth is a plain function
        meth   - string (symbol) naming the method or global function
        unique - if given, a name used to store this instance globally so
                 only one copy runs at a time
        params - extra positional arguments passed to meth

    Usage:
        p = Periodic(0.1, None, 'myfunc', p1, p2).start()
        ...
        p.stop()

    Return values from myfunc/mymethod control the period:
        'stop'     -> stop the sequence
        a number   -> set the period to that number
        None       -> continue with the current period
    """

    # Class-level registry for unique named instances
    _registry = {}

    def __init__(self, per, obj=None, meth=None, *params, unique=None, **kwargs):
        assert meth is None or isinstance(meth, str), \
            "meth must be a string (symbol) or None"
        self.period = per
        self.object = obj
        self.method = meth
        self.name = unique
        if unique is not None:
            Periodic._registry[unique] = self
        self.parameters = list(params)
        self.id = 0
        self.debug = None

    @classmethod
    def get_named(cls, name):
        """Return the named Periodic instance (like symbol_value in Serpent)."""
        return cls._registry.get(name)

    def print_name(self):
        return (self.name + " ") if (self.name is not None) else ""

    def stop(self):
        self.id += 1
        if self.debug:
            print(f"Periodic.stop {self.print_name()}{self.debug}")

    def start(self, delay=0):
        self.id += 1  # cancel previously scheduled launch
        if self.debug:
            start_time = vtime + delay
            print(f"Periodic.start {self.print_name()}{self.debug} "
                  f"period={self.period} "
                  f"method={self.method} start_time={start_time} "
                  f"parameters={self.parameters} id={self.id} sched={current}")
        cause(delay, self, 'launch', self.id)
        return self

    def action(self, *parameters):
        """Default action: delegate to object.method or global function."""
        return general_apply(self.object, self.method, list(parameters))

    def launch(self, prev_id):
        if prev_id != self.id:
            return
        if self.debug:
            print(f"Periodic running {self.method} with {self.parameters} "
                  f"({self.debug})")
        # If superseded by a newer instance with the same unique name, stop
        if self.name and Periodic._registry.get(self.name) is not self:
            return
        my_scheduler = current  # save in case action changes it
        rslt = sendapply(self, 'action', self.parameters)
        if rslt == 'stop':
            if self.debug:
                print(f"Periodic: {self.method} returned 'stop' "
                      f"({self.print_name()}{self.debug})")
            return  # no more iterations
        if isinstance(rslt, (int, float)):
            if self.debug:
                print(f"Periodic: {self.method} returned new period {rslt} "
                      f"({self.print_name()}{self.debug})")
            self.period = rslt
        if self.debug:
            print(f"Periodic scheduling next call to {self.method} in "
                  f"{self.period} ({self.print_name()}{self.debug}) "
                  f"{current}, {vtime + self.period}")
        my_scheduler.cause(self.period, self, 'launch', [self.id])

init()
