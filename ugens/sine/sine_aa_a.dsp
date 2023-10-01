declare name "sine";

declare description "Sine Unit Generator for Arco";
declare interpolated "amp";

import("stdfaust.lib");

process(freq, amp) =
    os.osc(freq)*amp;

