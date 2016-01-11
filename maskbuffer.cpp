#include "maskbuffer.h"

MaskBuffer::MaskBuffer(QImage const &image)
{
    bounds = image.size();
    data.reset(new uint8_t[bounds.width() * bounds.height()]);

    int xEnd = image.width();
    int yEnd = image.height();
    uint8_t *dataPtr = data.get();

    for (int iy = 0; iy < yEnd; ++iy)
        for (int ix = 0; ix < xEnd; ++ix)
        {
            *dataPtr++ = qGray(image.pixel(ix, iy));
        }
}

MaskBuffer MaskBuffer::invert() const
{
    MaskBuffer result;
    result.bounds = bounds;
    result.data.reset(new uint8_t[result.width() * result.height()]);

    size_t len = width() * height();
    for (size_t i = 0; i < len; ++i)
        result.data.get()[i] = 0xFF - data.get()[i];

    return result;
}
