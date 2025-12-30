#ifndef WAVES_H
#define WAVES_H

#define PI 3.14159265358979323846
#define freq(note) (2.0 * 440.0 * pow(2.0, ((note - 69.0) / 12.0)))

double sine(double x);
double sine8(double x);
double square(double x);
double saw(double x);

#endif