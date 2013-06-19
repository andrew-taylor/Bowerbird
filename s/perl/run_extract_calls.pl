#!/usr/bin/perl -w -I../perl
use Bowerbird;
use Cwd;

sub process(@);
sub process_file($);
$debug_level = 1;
my $forked = 0;
my $db = "units.db";
my $config = cwd()."/bowerbird_config";
$config = "bowerbird_config" if !-r $config;
$max_processes = 1;
exit(0) if !@ARGV;
while (@ARGV) {
	my $arg = shift @ARGV;
	if ($arg =~ /^-(\d+)$/) {
		$max_processes = $1;
	} elsif ($arg =~ /^--db$/) {
#		$arg = shift @ARGV or die;
#		$arg = cwd()."/$arg" if $arg !~ /\//;
		$db = $arg;
	} else {
		$arg = cwd()."/$arg" if $arg !~ /\//;
		push @pathnames, $arg;
	}
}
print STDERR "max_processes=$max_processes\n" if $debug_level;

if ($db !~ /^\//) {
	
}
process(@pathnames);
while ($forked) {
	$forked = 1 if wait == -1;
	$forked--;
}
exit 0;

sub process(@) {
	print STDERR "process(@_)\n" if $debug_level;
	foreach $pathname (@_) {
		my $dir = $pathname;
		if (-d $pathname) {
			process(glob("$pathname/*.*"));
			next;
		}
		$dir =~ s/\.\w+$//; 
		$dir =~ s/\@.*//;
		if (!-d $dir) {
			print STDERR "mkdir $dir\n" if $debug_level;
			mkdir $dir;
		}
		print STDERR "chdir $dir\n" if $debug_level;
		chdir $dir or die;
		process_file($pathname);
	}
}

sub process_file($) {
	my ($sound_file) = @_;
	return if $sound_file !~ /\.(wav|flac|shw|wv|sw)$/;
	return if -e "unit.html";
	while ($forked > $max_processes - 1) {
		$forked = 1 if wait == -1;
		$forked--;
	}
	$forked++;
	return if fork != 0;
	print STDERR "$sound_file\n";
	my $link = $sound_file;
	$link =~ s?.*/??;
	system "ln -s $sound_file $link" if !-l $link;
	my $processed_file = cwd()."/segment_0.wav";
	if (!-r "segment_0.wav") {
		if ($sound_file =~ /\.shw$/) {
			dsystem "shorten -x $sound_file -|sox --single-threaded -t .sw -r 16000 -c 2 - $processed_file highpass 10";
		} else {
#			dsystem "sox --single-threaded $sound_file -t .wav - highpass 10|segment_audio /dev/stdin >/dev/null";
#			dsystem "sox --single-threaded $sound_file -t .wav $processed_file highpass 10";
			dsystem "sox --single-threaded $sound_file -t .wav $processed_file highpass 10";
		}
	}
#	system "rm -f call*.details call*.track call*.spectrum call*.jpg";
	dsystem "extract_calls -v1 -C $config -ocall:database=$db $processed_file || rm -f unit.html";
	exit 0; # crucial
}
