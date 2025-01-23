/* report.cpp -- an include file to insert at the end of
 *     processing. It writes finish_time - start_time.
 */

// Roger B. Dannenberg
// Jan 2025

FILE *reportfile = fopen(RESULTS, "a");
if (!reportfile) fail();
fprintf(reportfile, "%g\n", finish_time - start_time);
fclose(reportfile);
