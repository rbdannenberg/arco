declare name "sine";
declare version "1.0";
declare author "RBD";
declare description "Sine Unit Generator for Arco";
declare interpolated "amp";

import("stdfaust.lib");

process(freq, amp) =
    os.osc(freq)*amp;
