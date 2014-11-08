#include "mypainttool.h"
#include "mypaintstrokecontext.h"
#include "paintutils.h"
#include <cmath>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QColor>
#include <QImage>

extern "C" {
#include "mypaint-brush.h"
};

using namespace std;

class MyPaintToolPrivate
{
public:
    MyPaintToolSettings originalSettings;
    MyPaintToolSettings currentSettings;
    QList<MaskBuffer> maskImages;

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

                if (mypaint_brush_setting_from_cname(settingName.toUtf8().constData()) < 0)
                    throw QString("Mypaint brush error, invalid setting:") + settingName;

                settings[settingName].first = settingObj.value("base_value").toDouble(0);

                QJsonObject inputsObj = settingObj.value("inputs").toObject();
                QJsonObject::iterator inputIter;

                for (inputIter = inputsObj.begin(); inputIter != inputsObj.end(); ++inputIter)
                {
                    QString    inputName  = inputIter.key();
                    QJsonArray inputArray = inputIter.value().toArray();

                    if (mypaint_brush_input_from_cname(inputName.toUtf8().constData()) < 0)
                        throw QString("Mypaint brush error, invalid setting:") + inputName;

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

        val = brushObj.value("image_settings");
        if (val.isObject())
        {
            QList<MaskBuffer> masks;
            const QJsonObject settingsObj = val.toObject();
            const QJsonArray maskArrayObj = settingsObj["masks"].toArray();
            for (auto const &maskObj: maskArrayObj)
            {
                QString maskStr = maskObj.toString();
                if (!maskStr.isEmpty())
                {
                    QByteArray decoded = QByteArray::fromBase64(maskStr.toUtf8());
                    QImage mask;
                    mask.loadFromData(decoded);
                    if (mask.isNull())
                        throw QString("MBI brush error, failed to decode mask");
                    mask.invertPixels();
                    masks.push_back(MaskBuffer(mask));
                }
                else
                    throw QString("MBI brush error, empty mask");
            }
            priv->maskImages = masks;
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

MyPaintTool::MyPaintTool(const MyPaintTool &tool) : priv(new MyPaintToolPrivate())
{
    *priv = *tool.priv;
}

MyPaintTool::~MyPaintTool()
{
    delete priv;
}

BaseTool *MyPaintTool::clone()
{
    return new MyPaintTool(*this);
}

void MyPaintTool::reset()
{
    priv->currentSettings = priv->originalSettings;
    priv->updateRadius();
}

void MyPaintTool::setToolSetting(QString const &name, QVariant const &value)
{
    if (name == QStringLiteral("size") && value.canConvert<float>())
    {
        //FIXME: Should query the min and max values instead of hardcoding
        float radius = qBound<float>(-2.0f, value.toFloat(), 6.0f);

        priv->setBrushValue("radius_logarithmic", radius);
        priv->updateRadius();
    }
    else if (name == QStringLiteral("opacity") && value.canConvert<float>())
    {
        //FIXME: Should query the min and max values instead of hardcoding
        float opacity = qBound<float>(0.0f, value.toFloat(), 2.0f);

        priv->setBrushValue("opaque", opacity);
    }
    else if (name == QStringLiteral("hardness") && value.canConvert<float>())
    {
        //FIXME: Should query the min and max values instead of hardcoding
        float hardness = qBound<float>(0.0f, value.toFloat(), 1.0f);

        priv->setBrushValue("hardness", hardness);
    }
    else if (name == QStringLiteral("lock_alpha") && value.canConvert<bool>())
    {
        float lock_alpha = value.toBool() ? 1.0f : 0.0f;

        priv->setBrushValue("lock_alpha", lock_alpha);
    }
    else
    {
        BaseTool::setToolSetting(name, value);
    }
}

QVariant MyPaintTool::getToolSetting(const QString &name)
{
    if (name == QStringLiteral("size"))
        return QVariant::fromValue<float>(priv->getBrushValue("radius_logarithmic"));
    else if (name == QStringLiteral("opacity"))
        return QVariant::fromValue<float>(priv->getBrushValue("opaque"));
    else if (name == QStringLiteral("hardness"))
        return QVariant::fromValue<float>(priv->getBrushValue("hardness"));
    else if (name == QStringLiteral("lock_alpha"))
        return QVariant::fromValue<bool>(priv->getBrushValue("lock_alpha") > 0.0f);
    else
        return BaseTool::getToolSetting(name);

}

QList<ToolSettingInfo> MyPaintTool::listToolSettings()
{
    QList<ToolSettingInfo> result;

    result.append(ToolSettingInfo::exponentSlider("size", "SizeExp", -2.0f, 6.0f));
    result.append(ToolSettingInfo::linearSlider("opacity", "Opacity", 0.0f, 1.0f));
    if (priv->maskImages.isEmpty())
        result.append(ToolSettingInfo::linearSlider("hardness", "Hardness", 0.0f, 1.0f));
    result.append(ToolSettingInfo::checkbox("lock_alpha", "Lock Alpha"));

    return result;

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

StrokeContext *MyPaintTool::newStroke(CanvasLayer *layer)
{
    MyPaintStrokeContext *stroke = new MyPaintStrokeContext(layer);

    stroke->fromSettings(priv->currentSettings);
    stroke->setMasks(priv->maskImages);

    return stroke;
}
