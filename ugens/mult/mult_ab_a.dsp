declare name "mult";

declare name "mult";
declare description "Mult(iply) Unit Generator for Arco";
declare interpolated "x1 x2";
declare terminate "x1 x2";

import("stdfaust.lib");

x2 = nentry("x2", 0, 0, 1, 0.1);

process(x1) = x1 * x2;
