#ifndef IMAGES_H
#define IMAGES_H

#include "ff.h"
#include <stdbool.h>

// File name of the image to display, in the card root
extern char images_path[FF_LFN_BUF + 1];

// Pick the image after the last shown one into images_path; false if the card root has no images
bool images_select_next(void);
// The image to be shown after images_path, without changing any state; false if it is not known yet
// (end of a random round) or the same image. Expects the card mounted; name holds FF_LFN_BUF + 1.
bool images_peek_next(char *name);
// A displayable image: a JPEG file (.jpg, .jpeg), not a folder, not hidden/system, and not a dot-file
// (skips macOS "._" companion files and similar)
bool images_is_image(const FILINFO *fno);
// Remember images_path as shown, so the next selection continues after it
void images_save_shown(void);

#endif
