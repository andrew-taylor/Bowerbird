#!/usr/bin/perl -w -I/home/andrewt/w/a/bowerbird/s/perl/
use Bowerbird;
use List::Util qw(first max maxstr min minstr reduce shuffle sum);
$debug_level = 1;
$| = 0;
open F,"|gnuplot -persist -geometry 2400x1000" or die;
foreach $track_file (@ARGV) {
	print F "re" if $first++;
	print F "plot '$track_file'   using 1  title '' with lines\n";
}
close F;

