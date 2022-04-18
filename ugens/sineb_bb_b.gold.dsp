declare name "sineb";
declare version "1.0";
declare author "RBD";
declare description "Sine Unit Generator for Arco";

import("stdfaust.lib");

freq = nentry("freq", 440, 1, 10000, 1);
amp = nentry("amp", 0.1, 0, 1, 0.01);

process =
    os.osc(freq)*amp;
