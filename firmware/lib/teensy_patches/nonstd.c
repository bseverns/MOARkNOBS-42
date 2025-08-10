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
    // If rounding adds an extra digit, compare using size_t so sign doesn't creep in.
    size_t diff = (size_t)(newDecimalPoint - tmp);
    size_t overflow_limit = (size_t)precision + 1;
    if (diff == overflow_limit) {
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
