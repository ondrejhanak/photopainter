#ifndef _RUN_FILE_H_
#define _RUN_FILE_H_

#include "DEV_Config.h"
#include "ff.h"

extern char pathName[FF_LFN_BUF + 1];

char sdTest(void);

void run_mount(void);
void run_unmount(void);

bool selectNextImage(void);
void saveShownImage(void);

#endif
