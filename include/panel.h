#ifndef PANEL_H
#define PANEL_H

// The frame's e-Paper panel, chosen at build time by the env in platformio.ini. Maps generic names
// onto that panel's driver; the hooks are the driver's weak functions, defined by the firmware.
#if defined(PANEL_7IN3F) // PhotoPainter, 7 colours
#include "EPD_7in3f.h"
#define PANEL_WIDTH EPD_7IN3F_WIDTH
#define PANEL_HEIGHT EPD_7IN3F_HEIGHT
#define PANEL_BLACK EPD_7IN3F_BLACK
#define PANEL_WHITE EPD_7IN3F_WHITE
#define PANEL_RED EPD_7IN3F_RED
#define PANEL_PALETTE palette_7color
#define panel_init EPD_7IN3F_Init
#define panel_display EPD_7IN3F_Display
#define panel_sleep EPD_7IN3F_Sleep
#define panel_busy_wait EPD_7IN3F_BusyWait // while the panel is busy (most of a refresh)
#define panel_refreshing EPD_7IN3F_Refreshing // once, when a refresh starts
#else
#error "No panel: build with an env from platformio.ini"
#endif

#endif
