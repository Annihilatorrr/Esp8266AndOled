#pragma once
#include <U8g2lib.h>

// Draw UTF-8 text inside a rectangle with word wrapping.
// (x, y) = baseline of first line.
// w/h = bounding box size in pixels.
// lineH = line height in pixels.
void drawWrappedUTF8(U8G2& u8g2,
                     int x, int y,
                     int w, int h,
                     int lineH,
                     const char* utf8);
