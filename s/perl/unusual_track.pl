#!/usr/bin/perl -w -I/home/andrewt/w/a/bowerbird/s/perl/
use Bowerbird;
use List::Util qw(first max maxstr min minstr reduce shuffle sum);
$debug_level = 1;
$| = 0;
use Statistics::Descriptive;
my $max_tracks_inspected = 10000000;
#$max_tracks_inspected = 10000;
@alphabet64 = (906,982,1114,1467,1660,1902,2209,2446,2589,2658,2699,2728,2750,2770,2787,2803,2818,2833,2848,2864,2881,2901,2926,2959,3014,3150,3330,3496,3683,3923,4202,4453,4536,4583,4616,4642,4664,4684,4702,4719,4737,4754,4773,4793,4815,4840,4869,4908,4976,5141,5662,6000,6143,6207,6246,6273,6294,6313,6336,6393,6898,7143,8373,10219);
$alphabet[$_/2] = $alphabet64[$_] foreach 0..63;
$tuple_length = 7;
my $bin_number = 0;
foreach $a (0..$#alphabet) {
	$bin[$bin_number++]  = $a ;
	$bin[$bin_number++]  = $a while $bin_number < $alphabet[$a];
}
$bin[$bin_number++]  = $#alphabet while $bin_number < 1.1*$alphabet[$#alphabet];
my (%sum_amplitude, $n_tracks, $total_count, %tuples);
#print "@bin\n";
foreach $details_file (map {-d $_ ? glob("$_/*.details") : $_} @ARGV) {
	my %track = read_track_file($details_file);
	my @f = ();
	push @f, $bin[$_] foreach @{$track{frequency}};
	my @t = ();
	foreach $i (0..(@f-$tuple_length)) {
		my $sum = 0;
		$sum = $sum*@alphabet+$f[$i+$_] foreach 0..($tuple_length-1);
		push @t, $sum;
		$count{$sum}++;
		$total_count++;
	}
	@{$tuples{$details_file}} = @t;
	$sum_amplitude{$details_file} = sum @{$track{amplitude}};
#	print "$details_file @t\n";
	last if $n_tracks++ > $max_tracks_inspected;
}
$inverse{$count{$_}}++ foreach keys %count;
printf "%5d %8d %.6f\n", $_, $inverse{$_}, $inverse{$_}/$total_count foreach sort {$a <=> $b} keys %inverse;
foreach $details_file (sort keys %tuples) {
	my $sum = sum (map $count{$_}, @{$tuples{$details_file}});
	$sum /= @{$tuples{$details_file}};
	$sum /= $total_count;
	$score{$details_file} = $sum_amplitude{$details_file}/$sum;
}
@scored_details_files = sort {$score{$b} <=> $score{$a}}  keys %score;
foreach $i (1..100) {
	my $details_file = $scored_details_files[$i];
	my %track = read_track_file($details_file);
	my @f = @{$track{amplitude}};
	my $base = $details_file;
	$base =~ s/.details//;
	my $dir = $base;
	$dir =~ s/\/[^\/]*$//;
	my ($source) = glob("$dir/*.wv");
	printf "%3d %7.3f $source $base.details $base.jpg $base.wav \n", $i, $score{$details_file};
}
