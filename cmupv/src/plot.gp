# command: gnuplot -c plot.gp datafile.dat
set title "Signal in ".ARG1
set xlabel "Time"
set ylabel "Amplitude"
plot ARG1 every ::2000::2050 with linespoints
pause -1 "Hit any key to continue"
