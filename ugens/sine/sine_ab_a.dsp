declare name "sine";

declare description "Sine Unit Generator for Arco";
declare interpolated "amp";

import("stdfaust.lib");

amp = nentry("amp", 0, 0, 1, 0.1);

process(freq) =
    os.osc(freq)*amp;

