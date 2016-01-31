#include "maskbuffer.h"
#include "lodepng.h"
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

    float y_bin_start = 0.0f;
    for (int y = 0; y < result.height(); ++y)
    {
        float y_bin_end = y_bin_start + bin_height;
        float x_bin_start = 0.0f;
        int iy_start = floor(y_bin_start);
        // y_bin_end is never less than 0.0, so capping at height() gurantees the value is always in range
        int iy_end = std::min<int>(floor(y_bin_end), height());

        for (int x = 0; x < result.width(); ++x)
        {
            float x_bin_end = x_bin_start + bin_width;
            float value = 0.0f;
            float weight = 0.0f;
            int ix_start = floor(x_bin_start);
            int ix_end = std::min<int>(floor(x_bin_end), width());

            for (int iy = iy_start; iy < iy_end; ++iy)
            {
                float y_before = y_bin_start - iy;
                float y_after  = iy + 1 - y_bin_end;
                float y_weight = (1.0f - std::max(y_before, 0.0f) - std::max(y_after, 0.0f));

                for (int ix = ix_start; ix < ix_end; ++ix)
                {
                    float p = data.get()[width() * iy + ix];
                    float x_before = x_bin_start - ix;
                    float x_after  = ix + 1 - x_bin_end;
                    float x_weight = (1.0f - std::max(x_before, 0.0f) - std::max(x_after, 0.0f));

                    float i_weight = x_weight * y_weight;

                    value = value + p * i_weight;
                    weight += i_weight;
                }
            }

            if (weight <= 0.0f)
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

QByteArray MaskBuffer::toPNG() const
{
    /* FIXME: This should be converted to use QImage::Format_Grayscale8 once we can require Qt 5.5 */
    size_t encodedSize;
    unsigned char *encoded;

    lodepng_encode_memory(&encoded, &encodedSize, constData(), width(), height(), LCT_GREY, 8);
    QByteArray encodedBytes = QByteArray((const char *)encoded, encodedSize);
    free(encoded);

    return encodedBytes;
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
