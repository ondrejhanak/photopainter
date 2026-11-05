// Host stand-in for include/work_memory.h: small, so the photo cap is reached with a few hundred files
#ifndef WORK_MEMORY_H
#define WORK_MEMORY_H

#include <stdint.h>

#define WORK_MEMORY_SIZE (2 * 1024)

extern uint8_t work_memory[WORK_MEMORY_SIZE];

#endif
