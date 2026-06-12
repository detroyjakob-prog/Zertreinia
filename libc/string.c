/* Freestanding implementations of the handful of libc routines we need.
 *
 * GCC sometimes emits calls to memset/memcpy/memmove/memcmp on its own
 * (e.g. for struct copies), so these must exist even though the kernel is
 * built with -ffreestanding. */

#include "string.h"

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--) {
        *d++ = (uint8_t)c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) {
        return dst;
    }
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        /* Copy backwards to handle overlapping regions. */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    while (n--) {
        if (*x != *y) {
            return (int)*x - (int)*y;
        }
        x++;
        y++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && (*a == *b)) {
        a++;
        b++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++)) { }
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *ret = dst;
    while (n && (*dst = *src)) {
        dst++;
        src++;
        n--;
    }
    while (n--) {
        *dst++ = '\0';
    }
    return ret;
}

char *strcat(char *dst, const char *src)
{
    char *ret = dst;
    while (*dst) {
        dst++;
    }
    while ((*dst++ = *src++)) { }
    return ret;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

int atoi(const char *s)
{
    int sign = 1;
    int value = 0;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }
    return sign * value;
}
