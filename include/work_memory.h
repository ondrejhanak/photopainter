#ifndef WORK_MEMORY_H
#define WORK_MEMORY_H

#include <stdint.h>

#define WORK_MEMORY_SIZE (48 * 1024)

// Scratch RAM shared by USB drive mode (read-ahead and write batch) and JPEG conversion, which
// never run at the same time; the heap has no room for it next to the panel's frame buffer
extern uint8_t work_memory[WORK_MEMORY_SIZE];

#endif
