#include <stddef.h>

void *memset(void *ptr, int value, size_t num)
{
    unsigned char *p = (unsigned char *)ptr;

    while (num--)
        *p++ = (unsigned char)value;

    return ptr;
}


void *memcpy(void *dst, const void *src, size_t num)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (num--)
        *d++ = *s++;

    return dst;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
    const unsigned char *a = (const unsigned char *)ptr1;
    const unsigned char *b = (const unsigned char *)ptr2;

    while (num--)
    {
        if (*a != *b)
            return (*a > *b) ? 1 : -1;

        a++;
        b++;
    }

    return 0;
}


char *strchr(const char *str, int c)
{
    while (*str)
    {
        if (*str == (char)c)
            return (char *)str;

        str++;
    }

    // C standard: strchr also finds the terminating '\0'
    if (c == '\0')
        return (char *)str;

    return NULL;
}