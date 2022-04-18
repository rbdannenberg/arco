declare name "mult";
declare version "1.0";
declare author "RBD";
declare description "Mult(iply) Unit Generator for Arco";
declare interpolated "x1 x2";

import("stdfaust.lib");

process(x1, x2) = x1 * x2;
