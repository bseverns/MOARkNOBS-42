/*
 * Teensy shim for non-standard libc helpers.
 * Only dtostrf() is implemented here because we needed to slap a warning
 * in the face. The real core has more goodies.
 */

#include <stdlib.h>
#include <string.h>

char *dtostrf(double val, signed char width, unsigned char precision, char *buf)
{
    int decpt, sign;
    char *tmp = fcvt(val, precision, &decpt, &sign);
    char *out = buf;
    if (sign) *out++ = '-';

    char *newDecimalPoint = tmp + decpt;
    // If rounding adds an extra digit, use unsigned math so the compiler keeps its cool.
    if ((unsigned)(newDecimalPoint - tmp) == (unsigned)(precision + 1)) {
        decpt++;
        newDecimalPoint--; // drop the overflowed digit
    }

    if (decpt <= 0) {
        *out++ = '0';
    } else {
        while (tmp < newDecimalPoint) *out++ = *tmp++;
    }

    *out++ = '.';
    while (*tmp) *out++ = *tmp++;
    *out = '\0';
    return buf;
}
