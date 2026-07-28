# pyarco design notes

## Key commands

If you want a somewhat interactive terminal-based
python client for Arco, you can:
    from terminput import TermInput
and
    def terminal_input_poll():
        input = io.getch()
        if input:
            print("terminal input got: ", repr(input))
            ... do something based on input char ...
and then early on, run:
    io = TermInput()
    io.start()
    sched.poll_function_add(terminal_input_poll)

Then, characters typed on the keyboard will be delivered
quickly to your program. To exit cleanly, have "q" (for quit)
run:
    sched.stop()
    io.stop()
    arco.reset(exit)

## Ugen IDs

IDs are much simpler under Python because Python has destructors. When
a Ugen is destroyed, we send a message to free the id from Python to
Arco. There are no UgenID objects as in Serpent, just integers, so no
polling for freed UgenID objects and tricky UgenID pool and
reclamation.

Ugens in Arco are reference counted, so if there is a reference from
Python, the Ugen will exist in Arco with an ID so Python can reference
it. If the Python shadow Ugen is freed, the Python object destructor
frees the ID, but there could be other references within Arco, so the
Ugen may continue to live until other references are freed, prolonging
the life of the Arco Ugen.

Also, the Arco Ugen can be referenced from other Arco Ugens. This will
also prolong the Arco Ugen, but this is a feature: If you set up a
patch from Python and free the Python objects, but the Arco Ugens are
connected to, say, Arco's output, then Arco will keep the objects
running. If you run the .play() method on a Python Ugen, you should
normally keep it around and eventually run .mute() to disconnect it
from audio output. (There are ways for Ugen trees to be
self-terminating as well.)

The number one rule here is: If the Python Ugen exists, the
corresponding Arco Ugen exists too.


### Registerd Actions

Registered actions contain a reference to a Python Ugen, and
registered actions are in a global `action_dict`, so they can, in
principle, last forever. This creates a circular dependency: A Python
Ugen holds a reference to an Arco Ugen. The Arco Ugen has an
`action_id` that indexes an action in `action_dict`, which references
the Python Ugen, completing the circle.

To allow Ugens to be freed, registered actions that map action IDs to
Python Ugen objects must use weaklinks so that the Ugen can be garbage
collected.

How do we reclaim action info? The same as in Serpent: when a Ugen is
freed by Arco, an action will be sent with the ACTION_FREE bit set,
telling the client to free the action.

The only difficulty is that when Arco is reset, we need to invalidate
all existing Ugen IDs. Serpent does this with an epoch number encoded
into the Ugen ID, stored as a (big) int.

With Python, we have no UgenID objects and no method needed to extract
the integer. Since these IDs can be stored anywhere, we cannot track
them down to "delete" them when Arco is reset.

We could simply not solve the problem and say the client is
responsible for not using an old Arco ID after Arco is reset.

A safer way, but relies on client code to check, is to store the epoch
number in the `id_num attribute`, using high-order bits, and require
access through a function that checks the epoch number and removes it,
like:
    `arco_ref(id_num) # check id_num and return true ID`
    
If client code fails to call `arco_ref()`, the `id_num` will be way
out of range, will do nothing except print an invalid id warning, and
the client will need to fix their code.  We can help discover problems
by starting with epoch 1. Then, if an integer without epoch number is
stored in `id_num` in place of a proper `id_num` with epoch, and if
`arco_ref(id_num)` is called, the epoch field (high order integer
bits) will be zero, and `arco_ref(id_num)` will return -1, making the
number invalid.  This will, hopefully, be caught and corrected early,
before it causes difficult-to-trace errors after an Arco reset.

A fortunate benefit of epoch numbers in `id_num`'s is that when you
reset Arco and advance the epoch number, and free all the local Python
Ugens (which are stored in a table for access by callback messages),
they will call their `__del__` methods and try to free their `id_num`
back to the pool. But now, the epoch part of the `id_num` will be
invalid, so it can be ignored.

For now, we make `arco_ref()` fail pretty hard by raising an exception
-- you should *never* touch an object or an `id_num` from another
epoch, so passing an invalid `id_num` to `arco_ref()` will probably
halt your program unless you catch the exception.  The exception
indicates that you somehow retained an object on the Python side after
the Arco-side object was deleted by resetting the server and moving to
the next epoch.
