package Bowerbird;
require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw/read_track_file read_labels_file read_details_file merge_intervals intervals_overlap read_attributes_file attributes_file_to_tuple distance_to_interval generate_attributes dsystem dprint dprintf distance time_to_date_format $bowerbird_bin_dir $scripts_dir $debug_level $html_base_dir $url_base $centre_x $centre_y/;
our $VERSION = 0.1;

sub read_track_file($);
sub read_labels_file($);
sub read_details_file($);
sub merge_intervals(@);
sub intervals_overlap(\@\@);
sub read_attributes_file($);
sub attributes_file_to_tuple($);
sub distance_to_interval($$@);
sub closest_interval($$@);
sub generate_attributes($);
sub delta_regression3($$\@@);
sub delta_regression($$\@@);
sub differentiate(@);
sub regression(\@\@);
sub dsystem(@);
sub dprint(@);
sub dprintf(@);
sub distance($$$$);
sub time_to_date_format($);

$scripts_dir = "/home/andrewt/w/a/bowerbird/s/scripts";
$platform = `uname -m|sed 's/^arm.*/arm/;s/.*86/x86/'`;
chomp $platform;
$bowerbird_bin_dir = "/home/andrewt/w/a/bowerbird/s/binaries-$platform/";
#$debug_level = 0;
$html_base_dir = "/var/www/gp1/";
$url_base = "http://glebe.homelinux.org/gp/";
($centre_x,$centre_y) = (48.098209,-103.915759);
1;

sub read_track_file($) {
	my ($file_base) = @_;
	$file_base =~ s/\.[a-z]*$//;
	my $details_file = "$file_base.details";
	my %track =  read_details_file($details_file);
	my $track_file = "$file_base.track";
	my $i = 0;
	open T, $track_file or die;
	while (<T>) {
		($track{frequency}[$i], $track{amplitude}[$i], $track{phase}[$i]) = split;
		$i++;
	}
	close T;
	$_ *= $track{sampling_rate} foreach @{$track{frequency}};
	return %track;
}

sub read_labels_file($) {
	my ($labels_file) = @_;
#	$labels_file =~ s/\.[a-z]*$/.txt/;
	local (*F, $_);
	my @intervals;
	open F, $labels_file or die "Can not open $labels_file:$!\n";
	while (<F>) {
		/^(\d+\.\d+)\s+(\d+\.\d+)\s/ or die "can not parse '$_'";
		push @intervals, [$1,$2];
	}
	close F;
	return  @intervals;
}

sub read_details_file($) {
	my ($details_file) = @_;
	$details_file =~ s/\.[a-z]*$/.details/;
	local (*F, $_);
	my %details = ();
	open F, $details_file or die "Can not open $details_file:$!\n";
	while (<F>) {
		/\s*(\S+)\s*=\s*(.*\S)/ or die;
		$details{$1} = $2;
	}
	close F;
	return %details;
}

sub merge_intervals(@) {
	local $_;
	my ($start,$finish) = (-1,-1);
	my @merged = ();
	foreach (sort {$$a[0] <=> $$b[0]} @_) {
		my ($s, $f) = @$_;
		if ($finish >= $s) {
			$finish  = $f;
		} else {
			push @merged, [$start,$finish] if $finish >= 0;
			($start,$finish)  = ($s,$f);
		}
	}
	push @merged, [$start,$finish] if $finish >= 0;
	return @merged;
}

sub intervals_overlap(\@\@) {
	my ($s1, $f1) = @{$_[0]};
	my ($s2, $f2) = @{$_[1]};
#	dprintf "intervals_overlap ($s1, $f1) ($s2, $f2) -> %d\n", ($s1 <= $f2 && $f1 >= $s2);
	return $s1 <= $f2 && $f1 >= $s2;
}

sub read_attributes_file($) {
	my ($attributes_file) = @_;
	$attributes_file =~ s/\.[a-z]*$/.attributes/;
	local (*F, $_);
	my %attributes = ();
	open F, $attributes_file or die "Can not open $attributes_file:$!\n";
	while (<F>) {
		/\s*(\S+)\s*=\s*(.*\S)/ or die;
		$attributes{$1} = $2;
	}
	close F;
	return %attributes;
}

sub attributes_file_to_tuple($) {
	my ($attributes_file) = @_;
	my @attributes;
	local (*F, $_);
	open F, $attributes_file or die "Can not open $_[0]:$!\n";
	push @attributes, $attributes_file;
	my %details = read_details_file($attributes_file);
	my $source = $details{'source'};
	if (!$source) {
		# temporary kludge
		$attributes_file =~ /(\d\d\d\d_\d\d_\d\d)_(\d\d)_(\d\d)/ or die;
		$source = "/home/andrewt/w/a/barren_grounds/analysed_sound/$1/${2}_$3.flac";
	}
	push @attributes, $source;
	push @attributes, $details{'channel'};
#	print STDERR "$attributes_file\n";
	push @attributes, defined $details{'offset'} ? $details{'offset'}: $details{'first_frame'} / $details{'sampling_rate'};
	while (<F>) {
		/=\s*(.*\S)/ or die;
		push @attributes, $1
	}
	close F;
	return @attributes;
}

sub distance_to_interval($$@) {
	local $_;
	my ($start, $finish, @intervals) = @_;
	my $best = -1;
	foreach (@intervals) {
		my ($s,$f) = @$_;
		my $d = 0;
		if ($finish < $s) {
			$d = $s - $finish;
		} elsif ($f < $start) {
			$d = $start - $f;
		} else {
			return 0;
		}
		$best = $d if $best < 0 || $d < $best;
	}
#	print STDERR "$start, $finish, @intervals -> $best\n";
#	exit if @intervals;
	return $best;
}

#sub distance_to_interval($$@) {
#        return @{closest_interval(@_)}[0];
#        return $best;
#}

sub closest_interval($$@) {
        local $_;
        my ($start, $finish, @intervals) = @_;
        my $best;
        my @best_interval;
        foreach (@intervals) {
                my ($s,$f) = @$_;
                my $d = 0;
                if ($finish < $s) {
                        $d = $s - $finish;
                } elsif ($f < $start) {
                        $d = $start - $f;
                } 
                next if defined $best && $d > $best;
                $best = $d;
                @best_interval = @$_;
                last if $d == 0;
        }
#       print STDERR "$start, $finish, @intervals -> $best\n";
#       exit if @intervals;
        return $best, @best_interval;
}

sub generate_attributes($) {
	my ($file_base) = @_;
	$file_base =~ s/\.[a-z]*$//;
#	my $attribute_file = "$file_base.attributes";
	my $details_file = "$file_base.details";
	my %details =  read_details_file($details_file);
	my $source = $details{'source'};
	die if !$source;
	my @attributes = ($details_file);
	push @attributes, $source;
	push @attributes, $details{'channel'};
	push @attributes, defined $details{'offset'} ? $details{'offset'}: $details{'first_frame'} / $details{'sampling_rate'};
	my $track_file = "$file_base.track";
	open T, $track_file or die;
	my @frequency0 = <T>;
	close T;
	chomp @frequency0;
	my $sampling_rate = $details{sampling_rate} || 16000;
	$_ *= $sampling_rate foreach @frequency0;
	my @x = (0..$#frequency0);
	my $length = defined $details{'length'} ? $details{'length'} : $details{'n_frames'} / $details{'sampling_rate'};
	my $seconds_per_step = $length/@frequency0;
	$_ *= $seconds_per_step foreach @x;
	my @spectrum;
	my $spectrum_file = "$file_base.spectrum";
	open S, $spectrum_file or die;
	my $i = 0;
	@{$spectrum[$i++]} = split /\s+/ while <S>;
	close S;
	my $width = @{$spectrum[0]};
	die "width not odd" if $width % 2 != 1;
	$width != $_ || die "spectrum not square  $width != @{$_}" foreach @spectrum;
	push  @attributes, "length=$length";
	push  @attributes,  delta_regression(3, "f", @x, @frequency0);
	foreach ((0.025,0.05,0.1,0.2,0.4)) {
		my $name = $_;
		$name =~ s/\.//;
		my $fixed_length = int $_/$seconds_per_step;
		$fixed_length = @frequency0 if @frequency0 < $fixed_length;
		push  @attributes, delta_regression(3, "f$name", @x, @frequency0[0..($fixed_length-1)]);
		push  @attributes, delta_regression(3, "amp$name", @x, map {$spectrum[$_][$width/2+1]} 0..($fixed_length-1));
		push  @attributes,  delta_regression(3, "fr$name", @x, reverse @frequency0[($#frequency0+1-$fixed_length)..$#frequency0]);
	}
	my $hertz_per_bin = ($details{'sampling_rate'}/2)/$details{'fft_n_bins'};
	my @b = (0..$width-1);
	$_ *= $hertz_per_bin foreach @b;
	push  @attributes,  delta_regression3(3, "s0", @b, @{$spectrum[0]});
	push  @attributes,  delta_regression3(3, "s1", @b, @{$spectrum[@spectrum/2]});
	push  @attributes,  delta_regression3(3, "s2", @b, @{$spectrum[$#spectrum]});
	use Math::FFTW;
	my @ydata = map {$frequency0[$_] || 0} 0..63;
	my $coefficients = Math::FFTW::fftw_dft_real2complex_1d(\@ydata);
	#		printf "%d %d\n", 0+@ydata,0+@$coefficients;
	foreach (0..@$coefficients/2-1) {
		my $a = $$coefficients[2*$_];
		my $b = $$coefficients[2*$_+1];
		push  @attributes, sprintf "fft%02d=%g", $_, sqrt($a*$a+$b*$b); 
	}
	return @attributes
}

sub delta_regression3($$\@@) {
	my ($n,$prefix,$x,@y) = @_;
#	print STDERR "$prefix y='@y'\n";
	return (
		delta_regression($n, "$prefix", @$x, @y),
		delta_regression($n, "${prefix}_a", @$x, @y[0..@y/2]),
		delta_regression($n, "${prefix}_b", @$x, @y[@y/2..$#y]),
		);
}

sub delta_regression($$\@@) {
	my ($n,$prefix,$x,@y) = @_;
	local $_;
	my @regression_field_names = ('mean','min','max','a','b','r');
	my @attributes = ();
	foreach $i (0..($n-1)) {
		my @r = regression(@$x, @y);
		push @attributes, "${prefix}_${i}_$regression_field_names[$_]=$r[$_]" foreach 0..$#regression_field_names;
		@y = differentiate(@y);
	}
	return @attributes;
}

sub differentiate(@) {
	my (@y) = @_;
	my @diff;
	foreach $i (0..($#y-1)) {
		$diff[$i] = $y[$i+1] - $y[$i];
	}
#	print STDERR "diff=@diff\n";
	return @diff;
}

sub regression(\@\@) {
	my ($x,$y) = @_;
	my $n = @$x;
	$n = @$y if @$y < $n;
#	printf STDERR "n=$n x=$x y=$y %d %d\n", 0+@$x, 0+@$y;
	return (0, 0, 0, 0, 0, 0) if $n == 0;
	my ($x_sum) = (0);
	$x_sum += $_ foreach @$x;
	my $x_mean = $x_sum/$n;
	my ($y_sum,$y_min,$y_max) = (0,$$y[0],$$y[0]);
	foreach (@$y) {
		$y_sum += $_;
		$y_min = $_ if $_ < $y_min;
		$y_max = $_ if $_ > $y_max;
	}
	my $y_mean = $y_sum/$n;
	
	my ($x_sum_squares,$y_sum_squares,$cross_squares) = (0,0,0);
#	print STDERR "n=$n x=@$x y=@$y\n";
	foreach $i (0..($n-1)) {
		$x_sum_squares += ($$x[$i] - $x_mean) * ($$x[$i] - $x_mean);
		$y_sum_squares += ($$y[$i] - $y_mean) * ($$y[$i] - $y_mean);
		$cross_squares += ($$x[$i] - $x_mean) * ($$y[$i] - $y_mean);
	}
	return ($y_mean, 0, 0, 0) if $x_sum_squares == 0;
	$b = $cross_squares/$x_sum_squares;
	$a = $y_mean - $b*$x_mean;
	$divisor = sqrt($x_sum_squares) * sqrt($y_sum_squares);
#	print STDERR "y_sum_squares=$y_sum_squares divisor=$divisor\n";
	return ($y_mean, ,$y_min, $y_max, $a, 0, 1) if $divisor == 0;
	$r = $cross_squares/$divisor;
	return ($y_mean, $y_min,$y_max, $a, $b, $r);
}

sub dsystem(@) {
	dprint "@_\n";
	my $path = $ENV{PATH};
	$ENV{PATH} = "$bowerbird_bin_dir:$scripts_dir:$ENV{PATH}";
	system "@_";
	$ENV{PATH} = $path;
}

sub dprint(@) {
	print STDERR @_ if $debug_level;
}

sub dprintf(@) {
	printf STDERR @_ if $debug_level;
}

sub distance($$$$) {
	my ($x1,$y1,$x2,$y2) = @_;
	return sqrt (($x1-$x2)*($x1-$x2)+($y1-$y2)*($y1-$y2));
}

sub time_to_date_format($) {
	my ($sec,$min,$hour,$mday,$month,$year) = localtime $_[0];
	return sprintf "%04d_%02d_%02d", $year+1900, $month+1, $mday;
}