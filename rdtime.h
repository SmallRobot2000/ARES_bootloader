#ifndef __RDTIME_H__
#define __RDTIME_H__

#include <stdint.h>
//100Mhz
uint64_t rdtime(void);

uint64_t time_ms(void);
uint32_t test_time(void);
uint64_t time_us(void);
#endif