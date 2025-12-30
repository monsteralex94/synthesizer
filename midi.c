#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "midi.h"

typedef struct {
    unsigned char status;
    unsigned char data1;
    unsigned char data2;
} MidiEvent;

typedef struct {
    int active;
    int note;
    int velocity;
    double start_time;
} ActiveNote;

#define MAX_CHANNELS 16
#define MAX_PITCHES 128

static unsigned int read_be32(FILE *f) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, f) != 4) return 0;
    return (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
}

static unsigned short read_be16(FILE *f) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, f) != 2) return 0;
    return (buf[0]<<8) | buf[1];
}

// Read a variable length quantity (VLQ) from the file
static unsigned int read_vlq(FILE *f) {
    unsigned int value = 0;
    unsigned char c;
    do {
        if (fread(&c, 1, 1, f) != 1) return 0;
        value = (value << 7) | (c & 0x7F);
    } while (c & 0x80);
    return value;
}

int put_midi(const char *filename, MidiNote *notes) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    char chunk[4];
    if (fread(chunk, 1, 4, f) != 4 || strncmp(chunk, "MThd", 4) != 0) {
        fclose(f);
        return -1;
    }

    if (read_be32(f) != 6) { fclose(f); return -1; }
    read_be16(f); // unsigned short format
    unsigned short ntracks = read_be16(f);
    unsigned short division = read_be16(f);

    if (division & 0x8000) {
        fclose(f);
        return -1; // SMPTE timecode not supported
    }

    // Default tempo: 120 BPM
    double bpm = 120.0;
    double seconds_per_tick = (60.0 / bpm) / division;

    int note_count = 0;
    ActiveNote active[MAX_CHANNELS][MAX_PITCHES];
    memset(active, 0, sizeof(active));

    for (int track = 0; track < ntracks; track++) {
        if (fread(chunk, 1, 4, f) != 4 || strncmp(chunk, "MTrk", 4) != 0) {
            fclose(f);
            return -1;
        }

        unsigned int track_len = read_be32(f);
        long track_end = ftell(f) + track_len;
        double time_ticks = 0;
        int running_status = 0;

        while (ftell(f) < track_end) {
            unsigned int delta = read_vlq(f);
            time_ticks += delta;

            unsigned char b;
            if (fread(&b, 1, 1, f) != 1) {
                perror("fread failed");
                exit(1);
            }

            if (b < 0x80) {
                if (!running_status) { fclose(f); return -1; }
                ungetc(b, f);
                b = running_status;
            } else {
                running_status = b;
            }

            unsigned char event_type = b & 0xF0;
            unsigned char channel = b & 0x0F;

            if (b == 0xFF) {  // Meta event
                unsigned char meta_type;
                if (fread(&meta_type, 1, 1, f) != 1) {
                    perror("fread failed");
                    exit(1);
                }
                
                unsigned int len = read_vlq(f);

                if (meta_type == 0x51 && len == 3) {
                    // Set Tempo: 3 bytes microseconds per quarter note
                    unsigned char tempo_bytes[3];
                    size_t bytes_read = fread(tempo_bytes, 1, 3, f);
                    if (bytes_read != 3) {
                        if (feof(f)) {
                            fprintf(stderr, "Unexpected end of file while reading tempo bytes\n");
                        } else if (ferror(f)) {
                            perror("Error reading tempo bytes");
                        }
                        exit(1);
                    }
                    // convert to microseconds
                    unsigned int us_per_quarter = (tempo_bytes[0] << 16) | (tempo_bytes[1] << 8) | tempo_bytes[2];
                    bpm = 60000000.0 / us_per_quarter;
                    seconds_per_tick = (60.0 / bpm) / division;

                    // skip remaining bytes if any (usually none here)
                    if (len > 3) fseek(f, len - 3, SEEK_CUR);
                } else {
                    // skip meta event data
                    fseek(f, len, SEEK_CUR);
                }
            } else if (event_type == 0x90 || event_type == 0x80) {
                unsigned char note, velocity;
                if (fread(&note, 1, 1, f) != 1) {
                    perror("Error reading note");
                }

                if (fread(&velocity, 1, 1, f) != 1) {
                    perror("Error reading note velocity");
                }

                if (event_type == 0x90 && velocity > 0) {
                    active[channel][note].active = 1;
                    active[channel][note].velocity = velocity;
                    active[channel][note].start_time = time_ticks * seconds_per_tick;
                } else {
                    if (active[channel][note].active) {
                        if (note_count < MAX_NOTES) {
                            notes[note_count].note = note;
                            notes[note_count].velocity = active[channel][note].velocity;
                            notes[note_count].start_time = active[channel][note].start_time;
                            notes[note_count].duration =
                                (time_ticks * seconds_per_tick) - active[channel][note].start_time;
                            note_count++;
                        }
                        active[channel][note].active = 0;
                    }
                }
            } else if (event_type == 0xA0 || event_type == 0xB0 || event_type == 0xE0) {
                fseek(f, 2, SEEK_CUR);
            } else if (event_type == 0xC0 || event_type == 0xD0) {
                fseek(f, 1, SEEK_CUR);
            } else if (b == 0xF0 || b == 0xF7) {
                unsigned int len = read_vlq(f);
                fseek(f, len, SEEK_CUR);
            } else {
                fclose(f);
                return -1;
            }
        }
    }

    fclose(f);
    return note_count;
}


double latest_moment(const MidiNote *notes, int num_notes) {
    double latest_moment = 0;
    double moment;

    for (int i = 0; i < num_notes; i++) {
        moment = notes[i].start_time + notes[i].duration;
        if (moment > latest_moment) {
            latest_moment = moment;
        }
    }

    return latest_moment;
}
