// Host stand-in for the Pico SDK's time: milliseconds set by the test
#include <stdint.h>
typedef uint64_t absolute_time_t;
absolute_time_t get_absolute_time(void);
static inline uint32_t to_ms_since_boot(absolute_time_t t)
{
    return (uint32_t)t;
}
