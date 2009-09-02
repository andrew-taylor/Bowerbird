#!/usr/bin/perl -w -I/home/andrewt/w/a/bowerbird/s/perl/
use Bowerbird;
use List::Util qw(first max maxstr min minstr reduce shuffle sum);
$debug_level = 1;
$| = 0;
use Statistics::Descriptive;

my $n_partitions = 64;
my $max_tracks_inspected = 100000;
my $frequency = Statistics::Descriptive::Full->new();
my $n_tracks = 0;
foreach $details_file (map {-d $_ ? glob("$_/*.details") : $_} @ARGV) {
	my %track = read_track_file($details_file);
	$frequency->add_data(@{$track{frequency}});
	my $stat = Statistics::Descriptive::Sparse->new();
	$stat->add_data(@{$track{amplitude}});
	$statistics{$details_file}{$_} = $stat->$_ foreach qw/min max mean sum/;
	last if $n_tracks++ > $max_tracks_inspected;
}
print "(";
printf "%d,", $frequency->percentile(100*$_/$n_partitions) foreach 1..$n_partitions;
print ")\n";
printf "%3s %d\n", $_, $frequency->percentile(100*$_/$n_partitions) foreach 0..$n_partitions;
$df = reduce { $statistics{$a}{max} > $statistics{$b}{max} ? $a : $b} (keys %statistics);
printf "Maximum amplitude is %s %s\n", $df, $statistics{$df}{max};
