#!/usr/bin/perl -w -I../perl
use Bowerbird;
sub process(@);
sub process_file($);
$debug_level = 1;
my $forked = 0;
my $cwd = `pwd`;
chomp $cwd;
my $db = "units.db";
my $config = "$cwd/bowerbird_config";
$config = "" if !-r $config;
$max_processes = 1;
exit(0) if !@ARGV;
if ($ARGV[0] =~ /^-(\d+)$/) {
	$max_processes = $1;
	shift @ARGV;
}
print STDERR "max_processes=$max_processes\n" if $debug_level;

process(@ARGV);
while ($forked) {
	$forked = 1 if wait == -1;
	$forked--;
}
exit 0;

sub process(@) {
	print STDERR "process(@_)\n" if $debug_level;
	foreach $pathname (@_) {
		my $cwd = `pwd`;
		chomp $cwd;
		$pathname = "$cwd/$pathname" if $pathname !~ /^\//;
		my $dir = $pathname;
		$dir =~ s/\/*$//;
		$dir =~ s/.*\///;
		$dir =~ s/\.\w+$// if ! -d $pathname; 
		$dir =~ s/\@.*// if ! -d $pathname;
		if (!-d $dir) {
			print STDERR "mkdir $dir\n" if $debug_level;
			mkdir $dir;
		}
		chdir $dir or die;
		if (-d $pathname) {
			process(glob("$pathname/*.*"));
		} else {
			process_file($pathname);
		}
		chdir $cwd;
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
	my $cwd = `pwd`;
	chomp $cwd;
	system "ln -s $sound_file $link" if !-l $link;
	if (!-r "segment_0.wav") {
		if ($sound_file =~ /\.shw$/) {
			dsystem "shorten -x $sound_file -|sox --single-threaded -t .sw -r 16000 -c 2 - $cwd/segment_0.wav highpass 10";
		} else {
#			dsystem "sox --single-threaded $sound_file -t .wav - highpass 10|segment_audio /dev/stdin >/dev/null";
#			dsystem "sox --single-threaded $sound_file -t .wav $cwd/segment_0.wav highpass 10";
			dsystem "sox --single-threaded $sound_file -t .wav $cwd/segment_0.wav highpass 10";
		}
	}
	system "rm -f call*.details call*.track call*.spectrum call*.jpg";
	my $db = "$cwd/../2.db";
	dsystem "extract_calls -v1 -C $config -ocall:database=$db $cwd/segment_0.wav || rm -f unit.html";
	exit 0; # crucial
}
