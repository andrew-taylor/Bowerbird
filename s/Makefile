SUB_DIRECTORIES = util spectral_analysis localization
COMPILE_TYPE = standard
ARCH = $(shell uname -m|sed 's/^arm.*/arm/;s/.*86/x86/')
T = binaries-$(ARCH)
SVN_REPOSITORY = /home/andrewt/w/a/call_catalog/svn
SCRIPTS_DIR = scripts
LIB = $T/libcall_catalog.a
LIB_DEBUG = $T/libcall_catalog-debug.a
LIB_PROFILE = $T/libcall_catalog-profile.a
PROTOTYPES_FILE = $T/prototypes.h
LOCAL_PROTOTYPES_FILE = $T/$S-prototypes.h
INCLUDES_PATH = -Iincludes -I$T -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include
COMMON_DEPENDENCIES = includes/bowerbird.h Mk
COMPILER = gcc
CC_COMMON_OPTS = '-std=gnu99' -fargument-noalias-global -Wall -Wstrict-prototypes -Werror $(INCLUDES_PATH) -DUSE_FFTW -DUSE_SQLITE -DUSE_KISS_FFT -DFIXED_POINT=32
CC = $(COMPILER) -O3 -g  -funroll-loops $(CC_COMMON_OPTS)
CC_DEBUG = $(COMPILER) -g -ggdb -fmudflap $(CC_COMMON_OPTS) -DNO_G_SLICE 
CC_PROFILE = $(COMPILER) -O3 -pg $(CC_COMMON_OPTS)
LINK_COMMON_OPTS = $(LIB)
LINK_FLAGS = $(LINK_COMMON_OPTS)
LINK_FLAGS_DEBUG = -lmudflap $(LINK_COMMON_OPTS)
LINK_FLAGS_PROFILE = -pg $(LINK_COMMON_OPTS)
EXTERNAL_LIBS = -lm -lglib-2.0 -lsndfile -lwavpack -lfftw3 -lgsl -lgslcblas -lsqlite3 -lsndfile -lfftw3 -lwavpack
LOCAL_FUNCTIONS = 
GLOBAL_FUNCTIONS = 
APPLICATIONS =  $T/extract_calls $T/sound_to_image $T/silence_removal $T/score_calls $T/localize
TEST_PROGRAMS =  $T/util-gnuplot_test $T/util-parameter_test $T/util-sound_io_test $T/util-approximate_log_test $T/spectral_analysis-power_test $T/spectral_analysis-estimate_sinusoid_parameters_test $T/spectral_analysis-track_sinusoids_test $T/spectral_analysis-peaks_test
util__GLOBAL_FUNCTIONS = general.c gnuplot.c xv.c parameter.c	sound_io.c approximate_log.c
LIB_MEMBERS = $T/util-general.o $T/util-gnuplot.o $T/util-xv.o $T/util-parameter.o $T/util-sound_io.o $T/util-approximate_log.o $T/spectral_analysis-kiss_fft.o $T/spectral_analysis-kiss_fftr.o $T/spectral_analysis-power.o $T/spectral_analysis-estimate_sinusoid_parameters.o $T/spectral_analysis-track_sinusoids.o $T/spectral_analysis-sinusoid.o $T/spectral_analysis-peaks.o $T/spectral_analysis-track.o $T/localization-select.o $T/localization-misc.o $T/localization-dataman.o $T/localization-sigproc.o $T/localization-geometry.o $T/localization-tdoa.o $T/localization-plotting.o $T/localization-kml.o
LIB_MEMBERS_DEBUG = $T/util-general-debug.o $T/util-gnuplot-debug.o $T/util-xv-debug.o $T/util-parameter-debug.o $T/util-sound_io-debug.o $T/util-approximate_log-debug.o $T/spectral_analysis-kiss_fft-debug.o $T/spectral_analysis-kiss_fftr-debug.o $T/spectral_analysis-power-debug.o $T/spectral_analysis-estimate_sinusoid_parameters-debug.o $T/spectral_analysis-track_sinusoids-debug.o $T/spectral_analysis-sinusoid-debug.o $T/spectral_analysis-peaks-debug.o $T/spectral_analysis-track-debug.o $T/localization-select-debug.o $T/localization-misc-debug.o $T/localization-dataman-debug.o $T/localization-sigproc-debug.o $T/localization-geometry-debug.o $T/localization-tdoa-debug.o $T/localization-plotting-debug.o $T/localization-kml-debug.o
LIB_MEMBERS_PROFILE = $T/util-general-profile.o $T/util-gnuplot-profile.o $T/util-xv-profile.o $T/util-parameter-profile.o $T/util-sound_io-profile.o $T/util-approximate_log-profile.o $T/spectral_analysis-kiss_fft-profile.o $T/spectral_analysis-kiss_fftr-profile.o $T/spectral_analysis-power-profile.o $T/spectral_analysis-estimate_sinusoid_parameters-profile.o $T/spectral_analysis-track_sinusoids-profile.o $T/spectral_analysis-sinusoid-profile.o $T/spectral_analysis-peaks-profile.o $T/spectral_analysis-track-profile.o $T/localization-select-profile.o $T/localization-misc-profile.o $T/localization-dataman-profile.o $T/localization-sigproc-profile.o $T/localization-geometry-profile.o $T/localization-tdoa-profile.o $T/localization-plotting-profile.o $T/localization-kml-profile.o
TEST_PROGRAMS_DEBUG = $T/util-gnuplot_test-debug $T/util-parameter_test-debug $T/util-sound_io_test-debug $T/util-approximate_log_test-debug $T/spectral_analysis-power_test-debug $T/spectral_analysis-estimate_sinusoid_parameters_test-debug $T/spectral_analysis-track_sinusoids_test-debug $T/spectral_analysis-peaks_test-debug
TEST_PROGRAMS_PROFILE = $T/util-gnuplot_test-profile $T/util-parameter_test-profile $T/util-sound_io_test-profile $T/util-approximate_log_test-profile $T/spectral_analysis-power_test-profile $T/spectral_analysis-estimate_sinusoid_parameters_test-profile $T/spectral_analysis-track_sinusoids_test-profile $T/spectral_analysis-peaks_test-profile
spectral_analysis__GLOBAL_FUNCTIONS = power.c estimate_sinusoid_parameters.c track_sinusoids.c sinusoid.c peaks.c track.c
spectral_analysis__LOCAL_FUNCTIONS = kiss_fft.c kiss_fftr.c
spectral_analysis__APPLICATIONS = extract_calls.c sound_to_image.c silence_removal.c score_calls.c
APPLICATIONS_DEBUG = $T/extract_calls-debug $T/sound_to_image-debug $T/silence_removal-debug $T/score_calls-debug $T/localize-debug
APPLICATIONS_PROFILE = $T/extract_calls-profile $T/sound_to_image-profile $T/silence_removal-profile $T/score_calls-profile $T/localize-profile
localization__LOCAL_FUNCTIONS = select.c misc.c dataman.c sigproc.c geometry.c tdoa.c plotting.c kml.c
localization__APPLICATIONS = localize.c

default: all all_debug all_profile

all: $(LIB) $(TEST_PROGRAMS) $(APPLICATIONS)

all_debug: $(LIB_DEBUG) $(TEST_PROGRAMS_DEBUG) $(APPLICATIONS_DEBUG)

all_profile: $(LIB_PROFILE) $(TEST_PROGRAMS_PROFILE) $(APPLICATIONS_PROFILE)

$(LIB): $(LIB_MEMBERS)
	ar rvs $@ $?

$(LIB_DEBUG): $(LIB_MEMBERS_DEBUG)
	ar rvs $@ $?

$(LIB_PROFILE): $(LIB_MEMBERS_PROFILE)
	ar rvs $@ $?

doxygen: 
	doxygen lib/doxygen_config

commit: 
	svn commit

test: all util/test spectral_analysis/test

clean: clobber
	@$(MAKE) all

clobber: 
	cd $T && ls|grep -v prototypes.h|xargs rm -f 

global: Mk $T

$T: 
	mkdir $T

echo_target: 
	@echo $T

util/test: $T/util-sound_io_test $T/util-sound_io_test-debug
	mkdir -p util/test_data
	util-sound_io_test-debug -V1 test_sound_files/ground_parrot_calls.wav test_sound_files/ground_parrot_calls.wv util/test_data/tmp.wav util/test_data/tmp.wv util/test_data/tmp1.wav util/test_data/tmp_output.wav <test_sound_files/ground_parrot_calls.wav >util/test_data/tmp_output.wav
	util-sound_io_test-debug test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav	test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wv  util/test_data/tmp.wav util/test_data/tmp.wv
	@rm -f util/test_data/tmp.*

$T/util-general.o: util/general.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-general-debug.o: util/general.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-general-profile.o: util/general.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-gnuplot.o: util/gnuplot.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-gnuplot-debug.o: util/gnuplot.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-gnuplot-profile.o: util/gnuplot.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-xv.o: util/xv.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-xv-debug.o: util/xv.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-xv-profile.o: util/xv.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-parameter.o: util/parameter.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-parameter-debug.o: util/parameter.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-parameter-profile.o: util/parameter.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-sound_io.o: util/sound_io.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-sound_io-debug.o: util/sound_io.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-sound_io-profile.o: util/sound_io.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-approximate_log.o: util/approximate_log.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-approximate_log-debug.o: util/approximate_log.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-approximate_log-profile.o: util/approximate_log.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-gnuplot_test.o: util/gnuplot_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-gnuplot_test-debug.o: util/gnuplot_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-gnuplot_test-profile.o: util/gnuplot_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-parameter_test.o: util/parameter_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-parameter_test-debug.o: util/parameter_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-parameter_test-profile.o: util/parameter_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-sound_io_test.o: util/sound_io_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-sound_io_test-debug.o: util/sound_io_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-sound_io_test-profile.o: util/sound_io_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-approximate_log_test.o: util/approximate_log_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/util-approximate_log_test-debug.o: util/approximate_log_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/util-approximate_log_test-profile.o: util/approximate_log_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/util-gnuplot_test: $T/util-gnuplot_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/util-gnuplot_test-debug: $T/util-gnuplot_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/util-gnuplot_test-profile: $T/util-gnuplot_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/util-parameter_test: $T/util-parameter_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/util-parameter_test-debug: $T/util-parameter_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/util-parameter_test-profile: $T/util-parameter_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/util-sound_io_test: $T/util-sound_io_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/util-sound_io_test-debug: $T/util-sound_io_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/util-sound_io_test-profile: $T/util-sound_io_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/util-approximate_log_test: $T/util-approximate_log_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/util-approximate_log_test-debug: $T/util-approximate_log_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/util-approximate_log_test-profile: $T/util-approximate_log_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

spectral_analysis/score_calls: $T/score_calls $T/score_calls-debug $T/score_calls-profile
	@rm -f gprof.out
	$T/score_calls-profile test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav
	@rm -f gprof.out

spectral_analysis/score_calls-profile: $T/score_calls $T/score_calls-debug $T/score_calls-profile
	@rm -f gmon.out
	$T/score_calls-profile test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav 
	gprof $T/score_calls-profile |more
	@rm -f gmon.out

spectral_analysis/score_calls-oprofile: $T/score_calls
	sudo opcontrol --start
	sudo opcontrol --separate=library
	sudo opcontrol --reset
	$T/score_calls test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wav 
	opcontrol --dump
	opannotate --source --output-dir=/tmp $T/score_calls
	opreport -g -l $T/score_calls 

spectral_analysis/silence_removal: $T/silence_removal $T/silence_removal-debug $T/silence_removal-profile
	silence_removal -V30 test_sound_files/ground_parrot_calls.wav /tmp/silence.wav
	@rm -f gprof.out
	silence_removal-profile test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wv /tmp/silence.wav
	gprof $T/silence_removal-profile 
	@rm -f gprof.out

spectral_analysis/test: spectral_analysis/unit_tests spectral_analysis/extract_calls_test

spectral_analysis/unit_tests: spectral_analysis/unit_tests0 spectral_analysis/unit_tests1

spectral_analysis/unit_tests0: all all_debug
	@for p in peaks_test power_test estimate_sinusoid_parameters_test track_sinusoids_test ; \
	do \
		$T/spectral_analysis-$$p  || exit 1;\
	done

spectral_analysis/unit_tests1: all all_debug
	@for p in power_test estimate_sinusoid_parameters_test track_sinusoids_test ; \
	do \
		$T/spectral_analysis-$$p  '-ospectral_analysis:use_fftw=0' || exit 1; \
	done

spectral_analysis/extract_calls_test: $T/extract_calls $T/extract_calls-debug
	@dir=/tmp/testdir$$$$; \
	mkdir -p $$dir; \
	C="$T/extract_calls -V1 -C spectral_analysis/test_data/bowerbird_config -ocall:output_directory:$$dir test_sound_files/ground_parrot_calls.wav"; \
	echo $$C; \
	$$C; \
	exit 0; \
	diff call.html ../test_data/ground_parrot_calls.wav.call.html || exit 1; \
	rm -f *.html call0*.*; \
	echo $T/extract_calls -V0 ../test_sound_files/ground_parrot_calls.wv; \
	$T/extract_calls -V0 ../../test_sound_files/ground_parrot_calls.wv; \
	diff call.html ../test_data/ground_parrot_calls.wav.call.html || exit 1; \
	rm -f *.html call0*.*; \
	echo valgrind -q --error-exitcode=1 extract_calls -V0 test_sound_files/ground_parrot_calls.wv; \
	valgrind -q --error-exitcode=1 extract_calls -V0 ../../test_sound_files/ground_parrot_calls.wv; \
	diff call.html ../test_data/ground_parrot_calls.wav.call.html || exit 1; \
	rm -f *.html call0*.*; \
	echo $T/extract_calls-debug -V0 '-ospectral_analysis:use_fftw=0' ../test_sound_files/ground_parrot_calls.wv; \
	$T/extract_calls-debug -V0 '-ospectral_analysis:use_fftw=0' ../../test_sound_files/ground_parrot_calls.wv; \
	diff call.html ../test_data/ground_parrot_calls.wav.call.html || exit 1; \
	rm -f *.html call0*.*; \
	cp ../test_data/bowerbird_config1 bowerbird_config; \
	echo $T/extract_calls -V0 test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wv; \
	$T/extract_calls -V0 ../../test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wv; \
	diff call.html ../test_data/barren_grounds0_2008_03_19_1205915224.787271@90.wv.call.html || exit 1; \
	cd ..; \
	rm -r $$dir;
	@echo /spectral_analysis/extract_calls extract_calls: OK

spectral_analysis/extract_calls_profile: $T/extract_calls-profile
	@dir=../testdir$$$$; \
	mkdir -p $$dir; \
	cd $$dir; \
	cp ../test_data/bowerbird_config .; \
	extract_calls-profile -V1 ../../test_sound_files/barren_grounds0_2008_03_19_1205915224.787271@90.wv; \
	gprof ../../$T/extract_calls-profile;  \
	diff call.html ../test_data/barren_grounds0_2008_03_19_1205915224.787271@90.wv.call.html; \
	rm -f *.html call0*.*; \
	cd ..; \
	rm -r $$dir

$T/spectral_analysis-kiss_fft.o: spectral_analysis/kiss_fft.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-kiss_fft-debug.o: spectral_analysis/kiss_fft.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-kiss_fft-profile.o: spectral_analysis/kiss_fft.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-kiss_fftr.o: spectral_analysis/kiss_fftr.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-kiss_fftr-debug.o: spectral_analysis/kiss_fftr.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-kiss_fftr-profile.o: spectral_analysis/kiss_fftr.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-power.o: spectral_analysis/power.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-power-debug.o: spectral_analysis/power.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-power-profile.o: spectral_analysis/power.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-estimate_sinusoid_parameters.o: spectral_analysis/estimate_sinusoid_parameters.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-estimate_sinusoid_parameters-debug.o: spectral_analysis/estimate_sinusoid_parameters.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-estimate_sinusoid_parameters-profile.o: spectral_analysis/estimate_sinusoid_parameters.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-track_sinusoids.o: spectral_analysis/track_sinusoids.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-track_sinusoids-debug.o: spectral_analysis/track_sinusoids.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-track_sinusoids-profile.o: spectral_analysis/track_sinusoids.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-sinusoid.o: spectral_analysis/sinusoid.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-sinusoid-debug.o: spectral_analysis/sinusoid.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-sinusoid-profile.o: spectral_analysis/sinusoid.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-peaks.o: spectral_analysis/peaks.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-peaks-debug.o: spectral_analysis/peaks.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-peaks-profile.o: spectral_analysis/peaks.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-track.o: spectral_analysis/track.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-track-debug.o: spectral_analysis/track.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-track-profile.o: spectral_analysis/track.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-power_test.o: spectral_analysis/power_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-power_test-debug.o: spectral_analysis/power_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-power_test-profile.o: spectral_analysis/power_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-estimate_sinusoid_parameters_test.o: spectral_analysis/estimate_sinusoid_parameters_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-estimate_sinusoid_parameters_test-debug.o: spectral_analysis/estimate_sinusoid_parameters_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-estimate_sinusoid_parameters_test-profile.o: spectral_analysis/estimate_sinusoid_parameters_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-track_sinusoids_test.o: spectral_analysis/track_sinusoids_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-track_sinusoids_test-debug.o: spectral_analysis/track_sinusoids_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-track_sinusoids_test-profile.o: spectral_analysis/track_sinusoids_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-peaks_test.o: spectral_analysis/peaks_test.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-peaks_test-debug.o: spectral_analysis/peaks_test.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-peaks_test-profile.o: spectral_analysis/peaks_test.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-extract_calls.o: spectral_analysis/extract_calls.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-extract_calls-debug.o: spectral_analysis/extract_calls.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-extract_calls-profile.o: spectral_analysis/extract_calls.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-sound_to_image.o: spectral_analysis/sound_to_image.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-sound_to_image-debug.o: spectral_analysis/sound_to_image.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-sound_to_image-profile.o: spectral_analysis/sound_to_image.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-silence_removal.o: spectral_analysis/silence_removal.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-silence_removal-debug.o: spectral_analysis/silence_removal.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-silence_removal-profile.o: spectral_analysis/silence_removal.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-score_calls.o: spectral_analysis/score_calls.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/spectral_analysis-score_calls-debug.o: spectral_analysis/score_calls.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/spectral_analysis-score_calls-profile.o: spectral_analysis/score_calls.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/spectral_analysis-power_test: $T/spectral_analysis-power_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/spectral_analysis-power_test-debug: $T/spectral_analysis-power_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/spectral_analysis-power_test-profile: $T/spectral_analysis-power_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/spectral_analysis-estimate_sinusoid_parameters_test: $T/spectral_analysis-estimate_sinusoid_parameters_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/spectral_analysis-estimate_sinusoid_parameters_test-debug: $T/spectral_analysis-estimate_sinusoid_parameters_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/spectral_analysis-estimate_sinusoid_parameters_test-profile: $T/spectral_analysis-estimate_sinusoid_parameters_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/spectral_analysis-track_sinusoids_test: $T/spectral_analysis-track_sinusoids_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/spectral_analysis-track_sinusoids_test-debug: $T/spectral_analysis-track_sinusoids_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/spectral_analysis-track_sinusoids_test-profile: $T/spectral_analysis-track_sinusoids_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/spectral_analysis-peaks_test: $T/spectral_analysis-peaks_test.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/spectral_analysis-peaks_test-debug: $T/spectral_analysis-peaks_test-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/spectral_analysis-peaks_test-profile: $T/spectral_analysis-peaks_test-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/extract_calls: $T/spectral_analysis-extract_calls.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/extract_calls-debug: $T/spectral_analysis-extract_calls-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/extract_calls-profile: $T/spectral_analysis-extract_calls-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/sound_to_image: $T/spectral_analysis-sound_to_image.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/sound_to_image-debug: $T/spectral_analysis-sound_to_image-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/sound_to_image-profile: $T/spectral_analysis-sound_to_image-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/silence_removal: $T/spectral_analysis-silence_removal.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/silence_removal-debug: $T/spectral_analysis-silence_removal-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/silence_removal-profile: $T/spectral_analysis-silence_removal-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

$T/score_calls: $T/spectral_analysis-score_calls.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/score_calls-debug: $T/spectral_analysis-score_calls-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/score_calls-profile: $T/spectral_analysis-score_calls-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)

localization/test: $T/localize
	mkdir -p /tmp/clicktracks/
	$T/localize localization/extra/listall

$T/localization-select.o: localization/select.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-select-debug.o: localization/select.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-select-profile.o: localization/select.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-misc.o: localization/misc.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-misc-debug.o: localization/misc.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-misc-profile.o: localization/misc.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-dataman.o: localization/dataman.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-dataman-debug.o: localization/dataman.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-dataman-profile.o: localization/dataman.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-sigproc.o: localization/sigproc.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-sigproc-debug.o: localization/sigproc.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-sigproc-profile.o: localization/sigproc.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-geometry.o: localization/geometry.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-geometry-debug.o: localization/geometry.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-geometry-profile.o: localization/geometry.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-tdoa.o: localization/tdoa.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-tdoa-debug.o: localization/tdoa.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-tdoa-profile.o: localization/tdoa.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-plotting.o: localization/plotting.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-plotting-debug.o: localization/plotting.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-plotting-profile.o: localization/plotting.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-kml.o: localization/kml.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-kml-debug.o: localization/kml.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-kml-profile.o: localization/kml.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localization-localize.o: localization/localize.c $(COMMON_DEPENDENCIES)
	$(CC) -c $< -o $@ 

$T/localization-localize-debug.o: localization/localize.c $(COMMON_DEPENDENCIES)
	$(CC_DEBUG) -c $< -o $@ 

$T/localization-localize-profile.o: localization/localize.c $(COMMON_DEPENDENCIES)
	$(CC_PROFILE) -c $< -o $@ 

$T/localize: $T/localization-localize.o $(LIB)
	$(CC) $< -o $@ $(LIB) $(LINK_FLAGS) $(EXTERNAL_LIBS)

$T/localize-debug: $T/localization-localize-debug.o $(LIB_DEBUG)
	$(CC_DEBUG) $< -o $@ $(LIB_DEBUG) $(LINK_FLAGS_DEBUG) $(EXTERNAL_LIBS)

$T/localize-profile: $T/localization-localize-profile.o $(LIB_PROFILE)
	$(CC_PROFILE) $< -o $@ $(LIB_PROFILE) $(LINK_FLAGS_PROFILE) $(EXTERNAL_LIBS)
