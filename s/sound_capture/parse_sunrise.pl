#!/usr/bin/perl -w
$month_offset = -6;
while (<>) {
	s/^\s+//;
	s/\s+$//;
	if (/^(\s*[A-Z]{3}){6}$/) {
		@months = split;
#		print STDERR "months=@months\n";
		$month_offset += @months;
		next;
	}
	/^(\d+)(\s+\d{4} \d{4}){6}/ or next;
	my $day = $1;
	s/^\s*\d+\s+//;
	my $column = 0;
	foreach (split /\s{3,10}/) {
#		print STDERR "_='$_'\n";
		my $month_index = $month_offset + $column++;
		my ($rise, $set) = split;
		$rise =~ /(\d\d)(\d\d)/;
		$rise[$month_index][$day] = $1*60 + $2;
		$set =~ /(\d\d)(\d\d)/;
		$set[$month_index][$day] = $1*60 + $2;
#		print STDERR "set[$month_index][$day]=$set[$month_index][$day]\n";
#		print STDERR "$months[$column] $month_index $day $rise $set\n";
	}	
}

$print_by_month = 0;
if ($print_by_month) {
	print "int est_sunrise_sunset[12][32][2] = {\n";
	foreach $month (0..11) {
		print "\t{{0,0}, ";
		foreach $day (1..31) {
			printf "{%d,%d}", $rise[$month][$day]||0, $set[$month][$day]||0;
			print "," if $day != 31;
		}
		print "}";
		print "," if $month != 11;
		print "\n";
	}
	print "};\n";
} else {
	my $n=0;
	print "int est_sunrise[366] = {";
	foreach $month (0..11) {
		foreach $day (1..31) {
			next if ! $rise[$month][$day];
			print "," if $n++;
			printf "%d", $rise[$month][$day];
		}
	} 
	print "};\n";
	$n=0;
	print "int est_sunset[366] = {";
	foreach $month (0..11) {
		foreach $day (1..31) {
			next if ! $set[$month][$day];
			print "," if $n++;
			printf "%d", $set[$month][$day];
		}
	} 
	print "};\n";
}
