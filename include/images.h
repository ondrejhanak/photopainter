#ifndef IMAGES_H
#define IMAGES_H

#include "ff.h"
#include <stdbool.h>

// File name of the image to display, in the card root
extern char images_path[FF_LFN_BUF + 1];
// Images in the card root, counted by the last selection
extern int images_count;

// Pick the next image into images_path: the next of the photos added in the latest drive mode while
// there are any, else the one after the last shown; false if the card root has no images
bool images_select_next(void);
// The image to be shown after images_path, without changing any state; false if it is not known yet
// (end of a random round) or the same image. Expects the card mounted; name holds FF_LFN_BUF + 1.
// Uses the work memory.
bool images_peek_next(char *name);
// A displayable image: a JPEG file (.jpg, .jpeg), not a folder, not hidden/system, and not a dot-file
// (skips macOS "._" companion files and similar)
bool images_is_image(const FILINFO *fno);
// Remember images_path as shown, so the next selection continues after it
void images_save_shown(void);
// Before the computer gets the card: save the list of photos, so images_find_new() can tell which
// were added. Uses the work memory.
void images_remember_photos(void);
// After drive mode: the photos added since images_remember_photos() are shown first, from the next
// selection on. Without additions, new photos of an earlier drive mode not shown yet stay first.
// Returns how many were added. Uses the work memory.
int images_find_new(void);

#endif
