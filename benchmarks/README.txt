Benchmarks to evaluate design choices in Arco.

arco<n>.cpp : baseline. Code is essentially Arco without audio IO,
    <n> is 2, 4, 8, 16, 32, 64, 128, 256
arcolike.cpp : to be included by arco<n>.cpp -- this is the shared implementation
arcolikecnt.cpp : counts how many times real_run is called as a sanity
    check; this is out-of-date and may not match arcolike.cpp exactly
block<n>.cpp : Study of all audio ugens at different block sizes,
    <n> is 2, 4, 8, 16, 32, 64, 128, 256
fastchannels.cpp : Study cost of bounds checking on state vectors,
    comparable to arco32.cpp
noclosure.cpp : Study cost of transitive closure - use compiled calls
    to real_run instead. Comparable to arco32.cpp
nopoly.cpp : Study cost of having polymorphic inputs.  Inner loops are
    run directly inside real_run() method. Comparable to noterminate.cpp
noterminate.cpp : Study cost of implementing termination
    semantics. Removes all termination checks. Comparable to arco32.cpp.
singlechannel.cpp : Study cost of multiple channels. Comparable to arco32.cpp.
singleugen.cpp : Study to collapse all ugens into one. Most comparable to
    nopoly.cpp if you just want to see what putting everything under
    one loop looks like.  Comparable to noterminate.cpp if you want to
    see how much speedup you can get if you give up dynamic patching
    and specialize the code.
fixphase.cpp : new idea to use fixed-point for phase
    accumulation. Saves 10% of entire computation (!) compared to arco32.cpp.
report.cpp - shared include file to append results to RESULTS
ugen.cpp, ugen.h : shared files for Ugen superclass implementation


Some results:
arco32: 0.9628
arcoopt with states.get_array() instead of &(states[0]): 0.9708 (!)
arcoopt with states.array using local vec.h and o2internal.h: 0.9663 (0.0177)
arcoopt where real_run computes first channel directly, then a for loop
    increments state and input/output pointers by strides and runs
    additional channels (but in this case it's just a test that skips all
    additional processing and returns): 0.9674 (.0052) NOT SIG
