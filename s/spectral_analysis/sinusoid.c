#include "i.h"

void
add_sinusoid(sample_t *samples, int n_samples, sinusoid_t s) {
	assert(s.frequency <= 0.5 && s.frequency >= 0);
	assert(s.amplitude <= 1 && s.frequency >= 0);
	for (int i = 0; i < n_samples; i++) {
		double d = s.phase + i*2*M_PI*s.frequency;
		samples[i] = double_to_sample_t(sample_t_to_double(samples[i]) + s.amplitude*sin(d));
//		assert(!isnan(samples[i]));
	}
}

void
add_sinusoid1(sample_t *samples, int n_samples, double amplitude, double phase, double frequency) {
	add_sinusoid(samples, n_samples, (sinusoid_t){amplitude, phase, frequency});
}

void
set_sinusoid(sample_t *samples, int n_samples, sinusoid_t s) {
	memset(samples, 0, n_samples*sizeof *samples);
	add_sinusoid(samples, n_samples, s);
}

void
set_sinusoid1(sample_t *samples, int n_samples, double amplitude, double phase, double frequency) {
	set_sinusoid(samples, n_samples, (sinusoid_t){amplitude, phase, frequency});
}


sinusoid_t
subtract_sinusoids(sinusoid_t s1, sinusoid_t s2) {
	return (sinusoid_t){s1.amplitude-s2.amplitude,s1.phase-s2.phase,s1.frequency-s2.frequency};
}

sinusoid_t
subtract_sinusoids1(sinusoid_t s1, sinusoid_t s2) {
	sinusoid_t d = subtract_sinusoids(s1, s2);
	if (s2.frequency)
		d.frequency /= s2.frequency;
	if (s2.amplitude)
		d.amplitude /= s2.amplitude;
	if (d.phase > M_PI)
		d.phase -= 2* M_PI;
	else if (d.phase < -M_PI)
		d.phase += 2* M_PI;
	return d;
}

sinusoid_t
add_sinusoids(sinusoid_t s1, sinusoid_t s2) {
	return (sinusoid_t){s1.amplitude+s2.amplitude,s1.phase+s2.phase,s1.frequency+s2.frequency};
}

sinusoid_t
multiply_sinusoids(sinusoid_t s1, sinusoid_t s2) {
	return (sinusoid_t){s1.amplitude*s2.amplitude,s1.phase*s2.phase,s1.frequency*s2.frequency};
}
