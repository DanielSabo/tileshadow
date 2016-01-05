#include "maskbuffer.h"
#include <qmath.h>

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

MaskBuffer MaskBuffer::downscale() const
{
    MaskBuffer result;
    result.bounds = {(bounds.width() % 2 + bounds.width()) / 2,
                     (bounds.height() % 2 + bounds.height()) / 2};
    result.data.reset(new uint8_t[result.width() * result.height()]);

    float bin_width = float(bounds.width()) / result.width();
    float bin_height = float(bounds.height()) / result.height();

    auto safeGet = [this](int x, int y) -> uint8_t {
        if (x < 0)
            x = 0;
        else if (x >= width())
            x = width() - 1;
        if (y < 0)
            y = 0;
        else if (y >= height())
            y = height() - 1;

        return data.get()[width() * y + x];
    };

    float y_bin_start = 0.0f;
    for (int y = 0; y < result.height(); ++y)
    {
        float y_bin_end = y_bin_start + bin_height;
        float x_bin_start = 0.0f;
        int iy_start = floor(y_bin_start);
        int iy_end = floor(y_bin_end);

        for (int x = 0; x < result.width(); ++x)
        {
            float x_bin_end = x_bin_start + bin_width;
            float value = 0.0f;
            float weight = 0.0f;
            int ix_start = floor(x_bin_start);
            int ix_end = floor(x_bin_end);

            for (int iy = iy_start; iy < iy_end; ++iy)
                for (int ix = ix_start; ix < ix_end; ++ix)
                {
                    float p = safeGet(ix, iy);
                    float y_before = y_bin_start - iy;
                    float y_after  = iy + 1 - y_bin_end;
                    float x_before = x_bin_start - ix;
                    float x_after  = ix + 1 - x_bin_end;

                    float i_weight = (1.0 - std::max(x_before, 0.0f) - std::max(x_after, 0.0f)) *
                                     (1.0 - std::max(y_before, 0.0f) - std::max(y_after, 0.0f));

                    value = value + p * i_weight;
                    weight += i_weight;
                }

            if (weight <= 0)
                value = 0.0f;
            else
                value /= weight;
            result.data.get()[result.width() * y + x] = std::min(std::max(0.0f, std::roundf(value)), 255.0f);

            x_bin_start += bin_width;
        }
      y_bin_start += bin_height;
    }

    return result;
}

MipSet<MaskBuffer> singleFromMask(MaskBuffer const &source)
{
    MipSet<MaskBuffer> result;
    result.mips.push_back(source);
    return result;
}

MipSet<MaskBuffer> mipsFromMask(MaskBuffer const &source)
{
    MipSet<MaskBuffer> result;
    result.mips.push_back(source);

    while (result.mips.back().width() > 4 && result.mips.back().height() > 4)
    {
        MaskBuffer scaled = result.mips.back().downscale();
        result.mips.push_back(scaled);
    }

    return result;
}
