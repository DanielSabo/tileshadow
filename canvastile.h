#ifndef CANVASTILE_H
#define CANVASTILE_H

#include <memory>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

static const int TILE_PIXEL_WIDTH  = 128;
static const int TILE_PIXEL_HEIGHT = 128;
static const int TILE_COMP_TOTAL = TILE_PIXEL_WIDTH * TILE_PIXEL_HEIGHT * 4;

/* From GEGL */
static inline int tile_indice (int coordinate, int stride)
{
    if (coordinate >= 0)
        return coordinate / stride;
    else
        return ((coordinate + 1) / stride) - 1;
}

class CanvasTile
{
public:
    CanvasTile();
    ~CanvasTile();

    float *mapHost();
    cl_mem unmapHost();
    void swapHost();

    void fill(float r, float g, float b, float a);
    std::unique_ptr<CanvasTile> copy();

    static int allocatedTileCount();
    static int deviceTileCount();

private:
  CanvasTile(const CanvasTile&);
  CanvasTile &operator=(const CanvasTile&);

  cl_mem  tileMem;
  float  *tileData;
};

#endif // CANVASTILE_H
