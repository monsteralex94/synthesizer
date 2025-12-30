#include <math.h>
#include "waves.h"

double sine(double x) {
    return sin(2 * PI * x);
}

double sine8(double x) {
    return (sine(x) + sine(x * 2) + sine(x * 3) + sine(x * 4) + sine(x * 5) + sine(x * 6) + sine(x * 7) + sine(x * 8)) / 2;
}

double square(double x) {
    return ((int)(x * 2) % 2) * 2 - 1;
}

double saw(double x) {
    return 2.0 * (x - floor(x)) - 1.0;
}