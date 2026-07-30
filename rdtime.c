#include "rdtime.h"

//100Mhz
#define TIMEBASE_FREQ 100000000ULL

uint64_t rdtime(void)
{
    uint32_t lo, hi;

    do {
        asm volatile ("rdtimeh %0" : "=r"(hi));
        asm volatile ("rdtime %0"  : "=r"(lo));
    } while (hi != ({
        uint32_t tmp;
        asm volatile ("rdtimeh %0" : "=r"(tmp));
        tmp;
    }));

    return ((uint64_t)hi << 32) | lo;
}



uint64_t time_ms(void)
{
    return rdtime() / (TIMEBASE_FREQ / 1000);
}

uint64_t time_us(void)
{
    return rdtime() / (TIMEBASE_FREQ / 1000000);
}