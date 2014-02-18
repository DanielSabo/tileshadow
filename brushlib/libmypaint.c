/* Include this file in your project
 * if you don't want to build libmypaint as a separate library
 * Note that still need to do -I./path/to/libmypaint/sources
 * for the includes here to succeed. */

#include "mapping.c"
#include "helpers.c"
// #include "brushmodes.c" // 16bpp blend modes used by mypaint-tiled-surface
#include "fifo.c"
// #include "operationqueue.c" // Used by mypaint-tiled-surface
#include "rng-double.c"
// #include "utils.c"
// #include "tilemap.c" // Used by operationqueue

// #include "mypaint.c"
#include "mypaint-brush.c"
#include "mypaint-brush-settings.c"
// #include "mypaint-fixed-tiled-surface.c"
#include "mypaint-surface.c"
// #include "mypaint-tiled-surface.c"
#include "mypaint-rectangle.c"
