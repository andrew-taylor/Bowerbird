import wave, struct, os
import numpy
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def generateSonogram(filename,  destination_dir, freq_scale="Linear",  fft_length=256):
	
	if os.path.isabs(filename):
		abs_filename = filename
	else:
		abs_filename = os.path.abspath(os.path.join('static', 'calls', filename))
	base_filename = os.path.splitext(filename)[0].replace('/', '_')
	sonogram_filename = "%s_%s_%d.png" %  (base_filename, freq_scale, fft_length)
	sonogram_pathname = os.path.join(destination_dir, sonogram_filename)
	
	# if file already exists and sonogram is not older than it then don't regenerate it
	if os.path.exists(sonogram_pathname) and (os.stat(sonogram_pathname).st_mtime > os.stat(abs_filename).st_mtime):
		return sonogram_filename

	wf = wave.open(abs_filename, 'rb')
	sample_width = wf.getsampwidth()
	sample_rate = wf.getframerate()
	n_frames = wf.getnframes()
	n_channels = wf.getnchannels()

	if sample_width != 2 or n_channels != 1: 
		raise ValueError('Unsupported format: must be 16bit mono')

	unpack_format = 'h'
	sample_offset = 0
	sample_max = 32768

	if n_frames < 10: 
		raise ValueError('Unsupported number of channels')

	n_ffts = min(2000, n_frames / fft_length)
	chunk = fft_length * n_channels
	signal =numpy.zeros(chunk*n_ffts)

	for i in range(n_ffts):
		f = wf.readframes(fft_length)
		signal[(i*chunk):((i+1)*chunk)] = struct.unpack("%d%c"%(chunk, unpack_format), f)
	wf.close()

	if sample_offset:
		signal -= sample_offset
	signal /= sample_max
	
	# ensure destination dir exists
	if not os.path.exists(destination_dir):
		os.mkdir(destination_dir, 0755) # second arg is octal mode (chmod-style)

	# clear the figure (some persistence seems to make this necessary)
	plt.clf()
	
	for channel in range(n_channels):
		plt.subplot(n_channels, 1, channel+1)
		Pxx, freqs, bins, im = plt.specgram(signal[channel::n_channels], NFFT=fft_length, Fs=sample_rate, noverlap=fft_length/2, scale_by_freq=False)
		plt.ylabel('Frequency (Hz)')
		plt.xlabel('Time (s)')
		if freq_scale == 'Logarithmic':
			plt.semilogy(basex=2)
	
	plt.savefig(sonogram_pathname)	
	return sonogram_filename
