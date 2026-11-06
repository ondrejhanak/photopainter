// Host stand-in for the Pico SDK's watchdog: scratch registers and the reset cause, set by the test
#include <stdbool.h>
#include <stdint.h>
typedef struct
{
    uint32_t scratch[8];
} watchdog_hw_t;
extern watchdog_hw_t *watchdog_hw;
bool watchdog_enable_caused_reboot(void);
static inline void watchdog_update(void) {}
