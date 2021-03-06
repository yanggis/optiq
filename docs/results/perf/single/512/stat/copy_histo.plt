reset
n=50#number of intervals
max=1650. #max value
min=0. #minvalue
width=(max-min)/n/2 #interval width

#function used to map a value to the intervals
hist(x,width)=width*floor(x/width/2.0)*2.0+width/2.0

set term postscript #output terminal and file
set tmarg 0
set bmarg 0
set lmarg 4
set rmarg 4
set output "copy_histo.ps"
set title "Distribution of number of copies over nodes" font ",22"

#set logscale x 2
set xrange [min:max]
set yrange [0:]

#to put an empty boundary around the
#data inside an autoscaled graph.
set offset graph 0.0,0.05,0.05,0.0

set key top right spacing 3
set key font ",22"

set xtics () font ",22"
set ytics () font ",22"

set boxwidth width*1.0

set style histogram clustered gap 2 title textcolor lt -1
set style data histograms

set style line 1 lc rgb "red"
set style line 2 lc rgb "blue"
set style line 3 lc rgb "green"

set style fill solid 1.0 #fillstyle
set tics out nomirror
set xlabel "Number of copies" font ",23"
set ylabel "Number of nodes" font ",23"
#count and plot
plot "opt_copies_87_64k.dat" u (hist($1,width)):($2) smooth freq w boxes ls 1 title "       Optimization", \
"heu_copies_87_64k.dat" u (hist($1,width)+width):($2) smooth freq w boxes ls 2 title "Heuristics"
