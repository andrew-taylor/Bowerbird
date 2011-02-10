#!/usr/bin/perl -w

#use CGI qw/:all/;
#use CGI::Carp qw(fatalsToBrowser carpout);
$| = 0;
$debug = 1;
$ENV{PATH} = "/usr/lib/bowerbird/beagleboard/bin/:$ENV{PATH}";
open LOG, ">/tmp/stream_log$$";
open LOG, ">/tmp/stream_log$$";
select LOG; $| = 1;
open STDERR, ">&LOG";
select STDERR; $| = 1;
select STDOUT; $| = 1;

print "Content-type: audio/mpeg\n\n";
#system "cat /tmp/stream1_9116.wav";
#exit;
chdir '/var/lib/bowerbird/data' or die "can not chdir /var/lib/bowerbird/data";
@days = glob '[0-9][0-9][0-9][0-9]_[0-9][0-9]_[0-9][0-9]';
chdir $days[$#days] or die;
$last_played_file = "";
for (1..60) {
	my @files = glob '[0-9]*.details';
	#print LOG "@files\n";
	my $file = $files[$#files];
	$file =~ s/details$/wv/;
	if ($file && $file ne $last_played_file) {
        my $command = "<$file wvunpack -q - >/tmp/stream0_$$.wav;resample -terse -to 32000 /tmp/stream0_$$.wav  /tmp/stream1_$$.wav >/dev/null 2>&1;shine /tmp/stream1_$$.wav - 2>/dev/null\n";
		print LOG $command if $debug;
		system $command;
		unlink "/tmp/stream0_$$.wav";
		unlink "/tmp/stream1_$$.wav";
		$last_played_file	= $file;
	} else {
        print LOG "sleeping\n" if $debug;
		sleep 1;
	}
}
