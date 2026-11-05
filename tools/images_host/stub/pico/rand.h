// Host stand-in for the Pico SDK's random numbers: deterministic, set by the test
#include <stdint.h>
uint32_t get_rand_32(void);
