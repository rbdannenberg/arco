declare name "multb";

declare name "mult";
declare description "Mult(iply) Unit Generator for Arco";
declare interpolated "x1 x2";

import("stdfaust.lib");

x1 = nentry("x1", 0, 0, 1, 0.1);
x2 = nentry("x2", 0, 0, 1, 0.1);

process = x1 * x2;
