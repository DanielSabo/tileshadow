#ifndef MYPAINTSTROKECONTEXT_H
#define MYPAINTSTROKECONTEXT_H

#include <QMap>
#include <QString>
#include <QPair>
#include "strokecontext.h"
#include "maskbuffer.h"

typedef QMap<QString, QPair<float, QMap<QString, QList<QPointF> > > > MyPaintToolSettings;

class MyPaintStrokeContextPrivate;
class MyPaintStrokeContext : public StrokeContext
{
public:
    MyPaintStrokeContext(CanvasLayer *layer);
    ~MyPaintStrokeContext() override;

    TileSet startStroke(QPointF point, float pressure) override;
    TileSet strokeTo(QPointF point, float pressure, float dt) override;

    bool fromSettings(MyPaintToolSettings const &settings);
    void fromDefaults();
    void setMasks(QList<MaskBuffer> const &masks = QList<MaskBuffer>());
    void setTexture(const MaskBuffer &texture, float textureOpacity);

    MyPaintStrokeContextPrivate *priv;
};

#endif // MYPAINTSTROKECONTEXT_H
