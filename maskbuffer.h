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

protected:
    QSize bounds;
    std::shared_ptr<uint8_t> data;
};

#endif // MASKBUFFER_H
