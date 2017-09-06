#ifndef CANVASTILE_H
#define CANVASTILE_H

#include <memory>
#include "blendmodes.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

static const int TILE_PIXEL_WIDTH  = 128;
static const int TILE_PIXEL_HEIGHT = 128;
static const int TILE_COMP_TOTAL = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;

static inline int tile_indice(int coordinate, int stride)
{
    // GCC and Clang are smart enough to turn this
    // into a bit shift at any optimization level.
    int n = (coordinate < 0) ? -1 : 0;
    return (coordinate - n) / stride + n;
}

class CanvasTile
{
public:
    CanvasTile();
    CanvasTile(const CanvasTile&) = delete;
    CanvasTile &operator=(const CanvasTile&) = delete;
    ~CanvasTile();

    float *mapHost();
    cl_mem unmapHost();
    void swapHost();

    void fill(float r, float g, float b, float a);
    void blendOnto(CanvasTile *target, BlendMode::Mode mode, float opacity);
    std::unique_ptr<CanvasTile> copy();

    static int allocatedTileCount();
    static int deviceTileCount();

private:
  cl_mem  tileMem;
  float  *tileData;
};

#endif // CANVASTILE_H
