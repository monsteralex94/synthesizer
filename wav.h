#ifndef WAV_H
#define WAV_H

#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS 1

void write_wav(const char *filename, double *samples, int num_samples);

#endif