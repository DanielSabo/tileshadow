#ifndef MASKBUFFER_H
#define MASKBUFFER_H

#include <QImage>
#include <memory>

class MaskBuffer {
public:
    MaskBuffer() : bounds(0, 0) {}
    MaskBuffer(QImage const &image);

    int width() const { return bounds.width(); }

    int height() const { return bounds.height(); }

    uint8_t const * constData() const {
        return data.get();
    }

    MaskBuffer invert() const;
    MaskBuffer downscale() const;
    QByteArray toPNG() const;

protected:
    QSize bounds;
    std::shared_ptr<uint8_t> data;
};

template <typename T>
class MipSet {
public:
    MipSet() : mips() {}
    MipSet(MipSet &from) = delete;
    MipSet& operator=(MipSet &from) = delete;
    MipSet(MipSet &&from) : mips(std::move(from.mips)) {}

    std::vector<T> mips;
    typedef typename std::vector<T>::iterator iterator;
    iterator begin() {
        return mips.begin();
    }
    iterator end() {
        return mips.end();
    }

    /* Try to find a mip who's largest dimension is at most 2*size,
     * otherwise return the smallest mip. */
    iterator bestForSize(int size)
    {
        if (mips.empty())
            return mips.end();

        size = size * 2;
        auto iter = mips.begin();
        while (iter + 1 != mips.end())
        {
            if (iter->width() <= size && iter->height() <= size)
                return iter;
            iter++;
        }
        return iter;
    }
};

/* Generate a MipSet with an unscaled copy of source as it's only level */
MipSet<MaskBuffer> singleFromMask(MaskBuffer const &source);

/* Generate a MipSet from source */
MipSet<MaskBuffer> mipsFromMask(MaskBuffer const &source);

#endif // MASKBUFFER_H
