declare name "mult";

declare name "mult";
declare description "Mult(iply) Unit Generator for Arco";
declare interpolated "x1 x2";
declare terminate "x1 x2";

import("stdfaust.lib");

process(x1, x2) = x1 * x2;
