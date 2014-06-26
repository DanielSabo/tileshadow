#include "mypainttool.h"
#include "mypaintstrokecontext.h"
#include <cmath>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QColor>

extern "C" {
#include "mypaint-brush.h"
};

using namespace std;

class MyPaintToolPrivate
{
public:
    QJsonDocument originalDoc;
    QJsonDocument currentDoc;

    float getBrushValue(QString name);
    void setBrushValue(QString name, float value);
    float getOriginalBrushValue(QString name);
};

void MyPaintToolPrivate::setBrushValue(QString name, float value)
{
    QJsonObject doc      = currentDoc.object();
    QJsonObject settings = doc.value("settings").toObject();
    QJsonObject setting  = settings.value(name).toObject();
    setting["base_value"] = QJsonValue(value);

    settings.insert(name, setting);
    doc.insert("settings", settings);
    currentDoc = QJsonDocument(doc);
}

float MyPaintToolPrivate::getOriginalBrushValue(QString name)
{
    return originalDoc.object()["settings"].toObject()[name].toObject()["base_value"].toDouble();
}

float MyPaintToolPrivate::getBrushValue(QString name)
{
    return currentDoc.object()["settings"].toObject()[name].toObject()["base_value"].toDouble();
}

MyPaintTool::MyPaintTool(const QString &path) : priv(new MyPaintToolPrivate())
{
    try
    {
        QFile file(path);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            throw QString("MyPaint brush error, couldn't open file");
        }

        QByteArray source = file.readAll();
        if (source.isNull())
        {
            throw QString("MyPaint brush error, couldn't read file");
        }

        QJsonDocument brushJsonDoc = QJsonDocument::fromJson(source);

        if (!brushJsonDoc.isObject())
        {
            throw QString("MyPaint brush error, JSON parse failed");
        }

        priv->originalDoc = brushJsonDoc;
        priv->currentDoc = priv->originalDoc;
    }
    catch (QString err)
    {
        qWarning() << (path + ":") << err;
    }
}

MyPaintTool::~MyPaintTool()
{
    delete priv;
}

void MyPaintTool::reset()
{
    priv->currentDoc = priv->originalDoc;
}

void MyPaintTool::setSizeMod(float mult)
{
    float radius = priv->getOriginalBrushValue("radius_logarithmic");
    radius = log(exp(radius) * mult);

    //FIXME: Should query the min and max values instead of hardcoding
    if (radius < -2.0f)
        radius = -2.0f;
    if (radius > 6.0f)
        radius = 6.0f;

    priv->setBrushValue("radius_logarithmic", radius);
}

float MyPaintTool::getPixelRadius()
{
    float radius = priv->getBrushValue("radius_logarithmic");
    return exp(radius);
}

void MyPaintTool::setColor(const QColor &color)
{
    priv->setBrushValue("color_h", color.hsvHueF());
    priv->setBrushValue("color_s", color.hsvSaturationF());
    priv->setBrushValue("color_v", color.valueF());
}

StrokeContext *MyPaintTool::newStroke(CanvasContext *ctx, CanvasLayer *layer)
{
    MyPaintStrokeContext *stroke = new MyPaintStrokeContext(ctx, layer);

    stroke->fromJsonDoc(priv->currentDoc);

    return stroke;
}
