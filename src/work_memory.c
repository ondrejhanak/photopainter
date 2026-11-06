#include "work_memory.h"
#include "jpeg_convert.h"

static_assert(JPEG_WORK_SIZE <= WORK_MEMORY_SIZE, "JPEG conversion needs more work memory");

alignas(8) uint8_t work_memory[WORK_MEMORY_SIZE]; // also holds uint64_t key lists
