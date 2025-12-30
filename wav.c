#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "wav.h"

void write_int32_le(FILE *f, uint32_t val) {
    fwrite(&val, 4, 1, f);
}

void write_int16_le(FILE *f, uint16_t val) {
    fwrite(&val, 2, 1, f);
}

void write_wav(const char *filename, double *samples, int num_samples) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        exit(1);
    }

    int byte_rate = SAMPLE_RATE * NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int block_align = NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int data_chunk_size = num_samples * NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int riff_chunk_size = 36 + data_chunk_size;

    // --- WAV HEADER ---

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    write_int32_le(f, riff_chunk_size);
    fwrite("WAVE", 1, 4, f);

    // fmt subchunk
    fwrite("fmt ", 1, 4, f);
    write_int32_le(f, 16);
    write_int16_le(f, 1);
    write_int16_le(f, NUM_CHANNELS);
    write_int32_le(f, SAMPLE_RATE);
    write_int32_le(f, byte_rate);
    write_int16_le(f, block_align);
    write_int16_le(f, BITS_PER_SAMPLE);

    // data subchunk
    fwrite("data", 1, 4, f);
    write_int32_le(f, data_chunk_size);

    int saturated = 0;

    // --- SAMPLE DATA ---
    for (int i = 0; i < num_samples; i++) {
        // Clamp and convert to 16-bit signed integer
        double val = samples[i];

        if (val > 1.0) { 
            val = 1.0;
            saturated = 1;
        } else if (val < -1.0) {
            val = -1.0;
            saturated = 1;
        }

        int16_t s = (int16_t)(val * 32767);
        write_int16_le(f, s);
    }

    if (saturated) {
        printf("ATTENTION! The output file is saturated. Consider lowering the volume parameter.\n");
    }

    fclose(f);
}