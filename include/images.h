#ifndef IMAGES_H
#define IMAGES_H

#include "ff.h"
#include <stdbool.h>

// File name of the image to display, in the card root
extern char images_path[FF_LFN_BUF + 1];

// Pick the image after the last shown one into images_path; false if the card root has no images
bool images_select_next(void);
// Remember images_path as shown, so the next selection continues after it
void images_save_shown(void);

#endif
