declare name "sine";

declare description "Sine Unit Generator for Arco";
declare interoplate "amp";

import("stdfaust.lib");

freq = nentry("freq", 0, 0, 1, 0.1);
amp = nentry("amp", 0, 0, 1, 0.1);

process =
    os.osc(freq)*amp;

