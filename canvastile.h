#ifndef CANVASTILE_H
#define CANVASTILE_H

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
    CanvasTile(int x, int y);
    ~CanvasTile();

    int     x;
    int     y;
    cl_mem  tileMem;
    float  *tileData;

    void mapHost(void);
    void unmapHost(void);

    void fill(float r, float g, float b, float a);
    CanvasTile *copy();

private:
  CanvasTile(const CanvasTile&);
  CanvasTile &operator=(const CanvasTile&);
};

#endif // CANVASTILE_H