declare name "sine";

declare description "Sine Unit Generator for Arco";
declare interpolated "amp";

import("stdfaust.lib");

freq = nentry("freq", 0, 0, 1, 0.1);

process(amp) =
    os.osc(freq)*amp;

