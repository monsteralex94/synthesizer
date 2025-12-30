#ifndef MIDI_H
#define MIDI_H

#define MAX_NOTES 1024

typedef struct {
    int note;
    int velocity;
    double start_time; // in seconds
    double duration;   // in seconds
} MidiNote;

int put_midi(const char *filename, MidiNote *notes);
double latest_moment(const MidiNote *notes, int num_notes);

#endif