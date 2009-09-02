#!/usr/bin/perl -w
sub output($$$\@);
$debug = 0;
$silence_threshold = 0.01*32768*65536; 
$minimum_silence_seconds = 0.001; 
$minimum_non_silence_seconds = 3; 
$maximum_processed_seconds = 8;
use Audio::SndFile;
use List::Util qw(first max maxstr min minstr reduce shuffle sum);
$buffer_size = 4096;
@in_files = @ARGV;
foreach $file (@in_files) {
	my $infile = Audio::SndFile->open("<", $file);
	my @non_silence = ();
	my @silence = ();
	my $outfile;
	$n_output_segments=0;
	my $maximum_silence = 0;
	$discarded_non_silence = 0;
	$discarded_silence = 0;
	my $n_samples = 0;
	while (@samples = $infile->unpackf_int($buffer_size)) {
#		printf "%d samples read from $file %s\n",@samples+0, join(",",@samples[0..15]) if $debug;
		if ($n_samples > $maximum_processed_seconds*$infile->samplerate * $infile->channels) {
			push @non_silence, @samples;
			next;
		}
		$n_samples += @samples;
		my @mean = ();
		for ($s = 0; $s < @samples; $s += $infile->channels) {
			$mean[$_] += $samples[$s+$_] foreach (0..($infile->channels-1));
		}
		$_ /= (@samples/$infile->channels) foreach @mean;
#		printf STDERR "mean=@mean n=%d\n", (@samples/$infile->channels);
		for ($s = 0; $s < @samples; $s += $infile->channels) {
			my @frame = @samples[$s..$s+$infile->channels-1];
#			foreach (0..$#frame) {printf STDERR "$silence_threshold $frame[$_] $mean[$_] %s\n", abs($frame[$_]-$mean[$_])};
#			print "frame=@frame\n" if $debug;
			my $frame_silent = 1;
			$frame_silent &= (abs($frame[$_]-$mean[$_]) <= $silence_threshold) foreach (0..$#frame);
			if ($frame_silent) {
				push @silence, @frame;
				next;
			}
			if (@silence) {
				$maximum_silence = max $maximum_silence, 0+@silence;
				printf STDERR "silence of %d frames detected\n", @silence/$infile->channels if $debug > 1;
				if (@silence < $minimum_silence_seconds * $infile->samplerate * $infile->channels) {
					push @non_silence, @silence;
				} else {
					$discarded_silence += @silence;
					printf STDERR "silence of %gs detected - preceding non-silence is %gs\n", @silence/($infile->samplerate * $infile->channels), @non_silence/($infile->samplerate * $infile->channels) if $debug;
					$outfile = output($file, $infile, $outfile, @non_silence);
					$outfile->close() if $outfile;
					$outfile = "";
				}
				@silence = ();
			}
			push  @non_silence, @frame;
		}	
	}
	printf STDERR "finish preceding non-silence is %gs\n", @non_silence/($infile->samplerate * $infile->channels) if $debug;
	$outfile = output($file, $infile, $outfile, @non_silence);
	$outfile->close() if $outfile;
	$infile->close();
	printf "$file - $n_output_segments segments, maximum silence %gs, %gs/%gs of non-silence/silence discarded\n", $maximum_silence/($infile->samplerate * $infile->channels), $discarded_non_silence/($infile->samplerate * $infile->channels), $discarded_silence/($infile->samplerate * $infile->channels);
}

sub output($$$\@) {
	my ($file,$infile,$outfile,$samples) = @_;
	if  (@$samples < $minimum_non_silence_seconds * $infile->samplerate * $infile->channels) {
		printf STDERR "$discarded_non_silence += %d\n", 0+@$samples if $debug;
		$discarded_non_silence += @$samples;
		@$samples = ();
		return $outfile;
	}
	if (!$outfile) {
		my $destination = $file;
		$destination = "segment.wav" if $file !~ /\./;
		$destination =~ s/(\.[^\.]*)$/_$n_output_segments.wav/;
		$n_output_segments++;
		print STDERR "opening $destination\n" if $debug;
		$outfile = Audio::SndFile->open(">", $destination,
#			type    => $infile->type,
#			subtype => $infile->subtype,
			type    => "wav",
			subtype => "pcm_16",
			samplerate => $infile->samplerate,
			channels => $infile->channels,
		);
	}
	printf STDERR "outputing %d samples\n",0+@$samples if $debug;
	$i = $outfile->packf_int(@$samples) or die "packf_int() returned '$i' file=$file samples=@$samples";
	@$samples = ();
	return $outfile;
}
