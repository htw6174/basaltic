#include "htw_core.h"

extern inline int min_int(int a, int b);

extern inline int max_int(int a, int b);

int htw_align(int value, int alignment) {
    int aligned = value; // return same size if it fits cleanly
    int diff = value % alignment;
    if (diff != 0) {
        // if there is an uneven remainder, increase to the next multiple of alignment
        aligned = value + (alignment - diff);
    }
    return aligned;
}

unsigned int htw_nextPow(unsigned int value) {
    unsigned int onbits = 0;
    unsigned int highestbit = 0;
    // check each bit to find the highest 1 bit, and the total number of 1 bits
    for (int i = 0; i < 32; i++) {
        unsigned int low = value & 1;
        onbits += low;
        if (low) highestbit = i;
        value = value >> 1;
    }
    // increase to next power of 2 if original value is not already a power of 2
    if (onbits > 1) highestbit++;
    return 1 << highestbit;
}

float lerp(float a, float b, float progress) {
    return (a * (1.0 - progress)) + (b * progress);
}

int lerp_int(int a, int b, double prog) {
    if (prog > 1) return b;
    if (prog < 0) return a;
    int range = b - a;
    return a + (prog * range);
}

double inverseLerp_int(int a, int b, int val) {
    if (val > b) return 1;
    if (val < a) return 0;
    return (double)(val - a) / (b - a);
}

int remap_int(int val, int oldMin, int oldMax, int newMin, int newMax) {
    double prog = inverseLerp_int(oldMin, oldMax, val);
    return lerp_int(newMin, newMax, prog);
}
