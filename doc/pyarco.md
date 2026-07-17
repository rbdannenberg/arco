# pyarco design notes

## Key commands

You can add a help string to the server by sending something like:
    o2lite.send_cmd("/host/command", 0, "s", " X - describe X")
where X is a letter (upper or lower, but not in "ABHpPQRStTU"
which already do things). The string is documentation printed
by the "H" command.

When the letter (X) is typed, the letter is sent as a string to
address "/actl/command" which takes a string parameter. You can
handle this in Python.

Unrecognized command letters in "A-Za-z0-9" are sent to
"/actl/command" even if there is no help string, e.g., you
can make a help string " 0-9 - play example sounds" rather
than 10 help strings starting with " 0 - play example 0",
and all 10 digits will generate "/actl/command" messages.
There is no check for whether the typed letter is a valid
or registered command. You might want Python to print
unrecognized commands to assist the user.

## Ugen IDs

It's much simpler under Python because Python has destructors. When
a Ugen is destroyed, we send a message to free the id from Python
to Arco. There are no UgenID objects, just integers, so no polling
for freed UgenID objects and tricky UgenID pool and reclamation.

Ugens in Arco are reference counted, so if there is a reference
from Python, the Ugen will exist with an ID so Python can reference
it. If the Python shadow Ugen is freed, the Python object destructor
frees the ID, but there could be other references within Arco, so
the Ugen may continue to live until other references are freed.

In the other direction, if all references to a Ugen are freed
in Arco, and the only remaining reference is from a shadow object
in Python, nothing happens. We do not free anything in Python or
even give any notification, so it is up to the client/user to
free references to the Python shadow Ugen to allow reclamation.

### Registerd Actions

Registered actions contain a reference to a Python Ugen, and
registered actions are in a global action\_dict, so they can,
in principle, last forever. This creates a circular dependency:
A Python Ugen holds a reference to an Arco Ugen. The Arco Ugen
has an action\_id that indexes an action in action\_dict, which
references the Python Ugen, completing the circle.

To allow Ugens to be freed, registered actions that map action
IDs to Python Ugen objects must use weaklinks so that the Ugen
can be garbage collected.

How do we reclaim action info? The same as in Serpent: when a Ugen
is freed by Arco, an action will be sent with the ACTION_FREE bit
set, telling the client to free the action.

The only difficulty is that when Arco is reset, we need to
invalidate all existing Ugen IDs. Serpent does this with an
epoch number encoded into the Ugen ID, stored as a (big) int.

With Python, we have no UgenID objects and no method needed
to extract the integer. Since these IDs can be stored anywhere,
we cannot track them down to "delete" them when Arco is reset.

We could simply not solve the problem and say the client is
responsible for not using an old Arco ID after Arco is reset.

A safer way, but relies on client code to check, is to store
the epoch number in the id_num attribute, using high-order
bits, and require access through
a function that checks the epoch number and removes it, like:
    arco_ref(id_num)  # check id_num and return true ID
If client code fails to call arco_ref(), the id_num will
be way out of range, will do nothing except print an invalid
id warning, and the client will need to fix their code.
We can help discover problems by starting with epoch 1. Then,
if an integer without epoch number is stored in id_num in place
of a proper id_num with epoch, and if arco_ref(id_num) is called,
the epoch field (high order integer bits) will be zero, and 
arco_ref(id_num) will return -1, making the number invalid.
This will, hopefully, be caught and corrected early, before
it causes difficult-to-trace errors after an Arco reset.

For now, we'll make arco_ref() fail pretty hard by raising an
exception -- you should *never* touch and object or an id_num
from another epoch, so passing an invalid id_num to arco_ref()
will probably halt your program unless you catch the exception.
The exception indicates that you somehow retained an object on
the Python side after the Arco-side object was deleted by
resetting the server and moving to the next epoch.




