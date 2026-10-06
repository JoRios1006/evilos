#pragma once
#include <stdint.h>
#include <stdint.h>

static inline uint64_t rdtsc(void) {
	uint32_t low;
    uint32_t high;
    __asm__ volatile ("lfence; rdtsc" : "=a"(low), "=d"(high) :: "memory");
    return ((uint64_t)high << 32) | low;
}
