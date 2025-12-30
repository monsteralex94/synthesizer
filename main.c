#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "midi.h"
#include "wav.h"
#include "waves.h"

#define NUM_OSCILLATORS 3
#define MAX_DETUNE_VOICES 16


typedef struct {
    double volume;
    double transpose;
    double (*waveform)(double);
    int detune_voices;
    double detune_amount;
    double detune_phase_offset[MAX_DETUNE_VOICES];
    double phase_randomization;
} oscillator;


// Synth's settings
double master_volume;
int attack;
int decay;
double sustain_volume;
int release;
double global_transpose;
oscillator oscillators[NUM_OSCILLATORS];


/* void print_midinote(const MidiNote *note) {
    printf("Note: %d\nVelocity: %d\nStart time: %f\nDuration: %f\n\n",
        note->note, note->velocity, note->start_time, note->duration);
} */


double normalized_waveform(double (*wave)(double), int x, double note, double phase_offset) {
    return wave(x * freq(note + global_transpose) / SAMPLE_RATE + phase_offset);
}


double oscillate(oscillator *osc, int x, int note) {
    double sum = normalized_waveform(osc->waveform, x, note + osc->transpose, 0);
    double single_detune = osc->detune_amount / osc->detune_voices;

    for (int i = 1; i < osc->detune_voices + 1; i++) {
        if (i % 2 == 1) {
            sum += normalized_waveform(osc->waveform, x, note + osc->transpose + single_detune * i,
                osc->detune_phase_offset[i]);
        } else {
            sum += normalized_waveform(osc->waveform, x, note + osc->transpose - single_detune * i,
                osc->detune_phase_offset[i]);
        }
    }

    return sum;
}


void put_samples(double *samples, MidiNote *notes, int num_notes) {
    for (int i = 0; i < num_notes; i++) {
        for (int j = 0; j < NUM_OSCILLATORS; j++) {
            oscillator *current_oscillator = &(oscillators[j]);
            MidiNote note = notes[i];

            int sample_x = 0;

            // attack phase
            int sample_start = note.start_time * SAMPLE_RATE;
            int sample_end = sample_start + attack;

            int sample_init_end = sample_start + note.duration * SAMPLE_RATE; // when release starts

            double attack_volume = 0;

            for (int i = sample_start; i < sample_end; i++) {
                samples[i] += oscillate(current_oscillator, sample_x, note.note)
                    * attack_volume * current_oscillator->volume * master_volume;
                attack_volume += current_oscillator->volume / attack;
                sample_x++;
            }

            // decay phase
            sample_start = sample_end;
            sample_end = sample_start + decay;

            double decay_volume = current_oscillator->volume;

            for (int i = sample_start; i < sample_end; i++) {
                samples[i] += oscillate(current_oscillator, sample_x, note.note)
                    * decay_volume * current_oscillator->volume * master_volume;
                decay_volume -= (current_oscillator->volume - sustain_volume) / decay;
                sample_x++;
            }

            // sustain phase
            sample_start = sample_end;
            sample_end = sample_init_end;

            for (int i = sample_start; i < sample_end; i++) {
                samples[i] += oscillate(current_oscillator, sample_x, note.note)
                    * sustain_volume * current_oscillator->volume * master_volume;
                sample_x++;
            }

            // release phase
            sample_start = sample_end;
            sample_end = sample_start + release;

            double release_volume = sustain_volume;

            for (int i = sample_start; i < sample_end; i++) {
                samples[i] += oscillate(current_oscillator, sample_x, note.note)
                    * release_volume * current_oscillator->volume * master_volume;
                release_volume -= sustain_volume / release;
                sample_x++;
            }
        }
    }
}


void set_params(char *filename) {
    FILE *f = fopen(filename, "r");

    double attack_s, decay_s, release_s;

    int num_success = fscanf(f, "Master Volume: %lf\nAttack: %lf\nDecay: %lf\nSustain: %lf\nRelease: %lf\nGlobal Transpose: %lf\n\n",
        &master_volume, &attack_s, &decay_s, &sustain_volume, &release_s, &global_transpose);

    if (num_success != 6) {
        printf("Syntax error in input file at global parameter %d\n", num_success + 1);
        exit(1);
    }

    attack = attack_s * SAMPLE_RATE;
    decay = decay_s * SAMPLE_RATE;
    release = release_s * SAMPLE_RATE;

    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        oscillator *current_oscillator = &(oscillators[i]);

        char waveform_c[7];

        num_success = fscanf(f, "Volume: %lf\nTranspose: %lf\nWaveform: %s\nDetune Voices: %d\nDetune Amount: %lf\nPhase Randomization: %lf\n\n", &(current_oscillator->volume), &(current_oscillator->transpose), &(waveform_c[0]), &(current_oscillator->detune_voices), &(current_oscillator->detune_amount), &(current_oscillator->phase_randomization));

        if (num_success != 6) {
            printf("Syntax error in input file at parameter %d of oscilator %d\n", num_success + 1, i + 1);
            exit(1);
        }

        if (!strcmp(waveform_c, "sine")) {
            current_oscillator->waveform = sine;
        } else if (!strcmp(waveform_c, "sine8")) {
            current_oscillator->waveform = sine8;
        } else if (!strcmp(waveform_c, "square")) {
            current_oscillator->waveform = square;
        } else if (!strcmp(waveform_c, "saw")) {
            current_oscillator->waveform = saw;
        } else {
            printf("Invalid waveform '%s' specified in oscilator %d!\n", waveform_c, i + 1);
            exit(1);
        }

        if (current_oscillator->detune_voices > MAX_DETUNE_VOICES) {
            printf("Maximum value for detune voices is %d, %d is too much!\n",
                MAX_DETUNE_VOICES, current_oscillator->detune_voices);
            exit(1);
        }

        for (int i = 0; i < current_oscillator->detune_voices; i++) {
            (current_oscillator->detune_phase_offset)[i] = ((double)rand() / (double)RAND_MAX) * 2 * PI * current_oscillator->phase_randomization;
        }
    }
}


int main(int argc, char **argv) {
    // arguments
    if (argc < 3) {
        printf("Wrong argument number!\n");
        printf("USAGE:\n");
        printf("    synthesizer <midi_file> <synth_parameter_file> <audio_output_file>\n");
        return 1;
    }

    // midi notes
    MidiNote *notes = malloc(sizeof(MidiNote) * MAX_NOTES);
    int num_notes = put_midi(argv[1], notes);

    // synth parameters and init phase offset for detuned oscilators
    set_params(argv[2]);
    srand(time(NULL));

    // create the samples and put them in the wave file
    double length = latest_moment(notes, num_notes);
    double *samples = calloc((int)(SAMPLE_RATE * length) + release, sizeof(double));

    if (!samples) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    put_samples(samples, notes, num_notes);
    write_wav(argv[3], samples, SAMPLE_RATE * length);

    // free everything
    free(notes);
    free(samples);

    printf("Done\n");

    return 0;
}
