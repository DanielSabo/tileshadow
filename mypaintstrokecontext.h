#ifndef MYPAINTSTROKECONTEXT_H
#define MYPAINTSTROKECONTEXT_H

#include <QMap>
#include <QString>
#include <QPair>
#include "strokecontext.h"

typedef QMap<QString, QPair<float, QMap<QString, QList<QPointF> > > > MyPaintToolSettings;

class MyPaintStrokeContextPrivate;
class MyPaintStrokeContext : public StrokeContext
{
public:
    MyPaintStrokeContext(CanvasLayer *layer);
    ~MyPaintStrokeContext();

    TileSet startStroke(QPointF point, float pressure);
    TileSet strokeTo(QPointF point, float pressure, float dt);

    bool fromSettings(MyPaintToolSettings const &settings);
    void fromDefaults();

    MyPaintStrokeContextPrivate *priv;
};

#endif // MYPAINTSTROKECONTEXT_H
