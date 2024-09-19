#ifndef OS_GFX_H
#define OS_GFX_H

#include "win32/os_gfx_win32.h"

struct OS_Area_Node {
    OS_Area_Node *next;
    Rect rect;
};

#endif // OS_GFX_H
