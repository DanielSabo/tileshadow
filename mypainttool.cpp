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
    MyPaintToolSettings originalSettings;
    MyPaintToolSettings currentSettings;

    float cursorRadius;

    float getBrushValue(QString name);
    void setBrushValue(QString name, float value);
    float getOriginalBrushValue(QString name);

    void updateRadius();
};

void MyPaintToolPrivate::setBrushValue(QString name, float value)
{
    currentSettings[name].first = value;
}

float MyPaintToolPrivate::getOriginalBrushValue(QString name)
{
    return originalSettings[name].first;
}

float MyPaintToolPrivate::getBrushValue(QString name)
{
    return currentSettings[name].first;
}

void MyPaintToolPrivate::updateRadius()
{
    cursorRadius = getBrushValue("radius_logarithmic");
    cursorRadius = exp(cursorRadius);
    cursorRadius = cursorRadius + 2 * cursorRadius * getBrushValue("offset_by_random");
//    cursorRadius = cursorRadius + 2 * cursorRadius * getBrushValue("offset_by_speed");
//    cursorRadius = cursorRadius + 2 * cursorRadius * getBrushValue("offset_by_speed_slowness");
}

MyPaintTool::MyPaintTool(const QString &path) : priv(new MyPaintToolPrivate())
{
    try
    {
        MyPaintToolSettings settings;

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

        QJsonObject brushObj = brushJsonDoc.object();
        QJsonValue  val = brushObj.value("version");
        int         brushVersion = brushObj.value("version").toDouble(-1);
        if (brushVersion != 3)
        {
            throw QString("MyPaint brush error, unknown version (") + QString(brushVersion) + ")";
        }

        val = brushObj.value("settings");
        if (val.isObject())
        {
            QJsonObject settingsObj = val.toObject();
            QJsonObject::iterator iter;

            for (iter = settingsObj.begin(); iter != settingsObj.end(); ++iter)
            {
                QString     settingName = iter.key();
                QJsonObject settingObj  = iter.value().toObject();

                settings[settingName].first = settingObj.value("base_value").toDouble(0);

                QJsonObject inputsObj = settingObj.value("inputs").toObject();
                QJsonObject::iterator inputIter;

                for (inputIter = inputsObj.begin(); inputIter != inputsObj.end(); ++inputIter)
                {
                    QString    inputName  = inputIter.key();
                    QJsonArray inputArray = inputIter.value().toArray();

                    QList<QPointF> mappingPoints;

                    int number_of_mapping_points = inputArray.size();

                    for (int i = 0; i < number_of_mapping_points; ++i)
                    {
                        QJsonArray point = inputArray.at(i).toArray();
                        float x = point.at(0).toDouble();
                        float y = point.at(1).toDouble();

                        mappingPoints.append(QPointF(x, y));
                    }

                    settings[settingName].second[inputName] = mappingPoints;
                }
            }
        }

        priv->originalSettings = settings;
    }
    catch (QString err)
    {
        qWarning() << (path + ":") << err;
        priv->originalSettings = MyPaintToolSettings();
    }

    priv->currentSettings = priv->originalSettings;
    priv->updateRadius();
}

MyPaintTool::~MyPaintTool()
{
    delete priv;
}

void MyPaintTool::reset()
{
    priv->currentSettings = priv->originalSettings;
    priv->updateRadius();
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
    priv->updateRadius();
}

float MyPaintTool::getPixelRadius()
{
    return priv->cursorRadius;
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

    stroke->fromSettings(priv->currentSettings);

    return stroke;
}
