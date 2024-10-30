declare name "sine";
freq = nentry("freq", 0, 0, 1, 0.1);


declare description "Sine Unit Generator for Arco";
declare interpolated "amp";

import("stdfaust.lib");

process(amp) =
    os.osc(freq)*amp;

