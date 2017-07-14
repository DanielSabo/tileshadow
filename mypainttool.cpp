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
#include <QImage>

extern "C" {
#include "mypaint-brush.h"
};

namespace {
bool validateMapping(QList<QPointF> const &mapping)
{
    if (mapping.isEmpty())
        return true;
    else if (mapping.size() < 2)
        return false;

    auto iter = mapping.begin();
    QPointF last = *iter++;

    for (; iter != mapping.end(); iter++)
    {
        if (last.x() > iter->x())
            return false;
        last = *iter;
    }

    return true;
}
}

using namespace std;

typedef QMap<QString, QList<QPointF>> BrushValueMapping;

class MyPaintToolPrivate
{
public:
    MyPaintToolSettings settings;
    QList<MaskBuffer> maskImages;
    MaskBuffer texture;
    float textureOpacity = 1.0f;
    bool isolate = false;
    QString description;
    QString notes;

    float cursorRadius;

    float getBrushValue(QString name);
    void setBrushValue(QString name, float value);
    float getOriginalBrushValue(QString name);
    BrushValueMapping getBrushMapping(QString name);
    void setBrushMapping(QString name, BrushValueMapping &&mapping);

    void updateRadius();
};

void MyPaintToolPrivate::setBrushValue(QString name, float value)
{
    settings[name].first = value;
}

float MyPaintToolPrivate::getBrushValue(QString name)
{
    return settings[name].first;
}

QMap<QString, QList<QPointF> > MyPaintToolPrivate::getBrushMapping(QString name)
{
    return settings[name].second;
}

void MyPaintToolPrivate::setBrushMapping(QString name, BrushValueMapping &&mapping)
{
    settings[name].second = mapping;
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

        val = brushObj.value("description");
        if (val.isString())
        {
            priv->description = val.toString();
        }

        val = brushObj.value("notes");
        if (val.isString())
        {
            priv->notes = val.toString();
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

                    if (validateMapping(mappingPoints))
                    {
                        settings[settingName].second[inputName] = mappingPoints;
                    }
                    else
                    {
                        throw QString("Mypaint brush error, invalid mapping:") + settingName + "," + inputName;
                    }
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
                    masks.push_back(MaskBuffer(mask).invert());
                }
                else
                    throw QString("MBI brush error, empty mask");
            }
            priv->maskImages = masks;

            QString textureStr = settingsObj.value("texture").toString();
            if (!textureStr.isEmpty())
            {
                QByteArray decoded = QByteArray::fromBase64(textureStr.toUtf8());
                QImage mask;
                mask.loadFromData(decoded);
                if (mask.isNull())
                    throw QString("MBI brush error, failed to decode texture");
                priv->texture = MaskBuffer(mask);
            }

            priv->textureOpacity = settingsObj.value("texture_opacity").toDouble(1.0);

            QJsonValue isolateValue = settingsObj.value("isolate");
            if (isolateValue.isDouble())
                priv->isolate = settingsObj.value("isolate").toDouble(0.0);
            else
                priv->isolate = settingsObj.value("isolate").toBool(false);
        }

        priv->settings = settings;
    }
    catch (QString err)
    {
        qWarning() << (path + ":") << err;
        priv->settings = MyPaintToolSettings();
    }

    priv->updateRadius();
}

MyPaintTool::MyPaintTool(const MyPaintTool &tool) : priv(new MyPaintToolPrivate())
{
    *priv = *tool.priv;
}

MyPaintTool::~MyPaintTool()
{
}

std::unique_ptr<BaseTool> MyPaintTool::clone()
{
    return std::unique_ptr<MyPaintTool>(new MyPaintTool(*this));
}

namespace {
static const QString MAPPING_SUFFIX = QStringLiteral(":mapping");
static const QStringList BOOL_SETTING_NAMES = {"lock_alpha", "eraser"};

const MyPaintBrushSettingInfo *getMypaintBrushSettingInfo(QString const &name)
{
    MyPaintBrushSetting settingId = mypaint_brush_setting_from_cname(name.toUtf8().constData());
    if (settingId != (MyPaintBrushSetting)-1)
        return mypaint_brush_setting_info(settingId);
    return nullptr;
}
}

void MyPaintTool::setToolSetting(QString const &inName, QVariant const &value)
{
    QString name = inName;
    bool isMapping = name.endsWith(MAPPING_SUFFIX);

    if (isMapping)
        name.chop(MAPPING_SUFFIX.length());

    if (name == QStringLiteral("size"))
        name = QStringLiteral("radius_logarithmic");

    if (name == QStringLiteral("masks"))
    {
        priv->maskImages = value.value<QList<MaskBuffer>>();
        return;
    }
    else if (name == QStringLiteral("texture"))
    {
        priv->texture = value.value<MaskBuffer>();
        return;
    }
    else if (name == QStringLiteral("texture_opacity"))
    {
        priv->textureOpacity = qBound<float>(0.0f, value.toFloat(), 1.0f);
        return;
    }
    else if (name == QStringLiteral("isolate"))
    {
        priv->isolate = value.toBool();
        return;
    }
    else if (name == QStringLiteral("description"))
    {
        priv->description = value.toString();
        return;
    }
    else if (name == QStringLiteral("notes"))
    {
        priv->notes = value.toString();
        return;
    }
    else if (isMapping)
    {
        auto iter = priv->settings.find(name);
        if (iter != priv->settings.end())
        {
            BrushValueMapping mappingSet = value.value<BrushValueMapping>();
            for (auto const &mapping: mappingSet)
            {
                if (!validateMapping(mapping))
                {
                    qDebug() << "Mapping for" << name << "contains invalid values:" << mapping;
                    return;
                }
            }

            iter.value().second = mappingSet;
        }
        else
        {
            qDebug() << "Can't set mapping for" << name << "because it has no base value";
        }
        return;
    }
    else if (auto settingInfo = getMypaintBrushSettingInfo(name))
    {
        float floatValue;
        if (BOOL_SETTING_NAMES.contains(name))
            floatValue = value.toBool() ? 1.0f : 0.0f;
        else
            floatValue = value.toFloat();

        floatValue = qBound<float>(settingInfo->min, floatValue, settingInfo->max);
        priv->setBrushValue(name, floatValue);
        priv->updateRadius();
        return;
    }

    BaseTool::setToolSetting(name, value);
}

QVariant MyPaintTool::getToolSetting(const QString &inName)
{
    QString name = inName;
    bool isBoolean = false;
    bool isMapping = name.endsWith(MAPPING_SUFFIX);

    if (isMapping)
        name.chop(MAPPING_SUFFIX.length());

    if (name == QStringLiteral("masks"))
        return QVariant::fromValue(priv->maskImages);
    else if (name == QStringLiteral("texture"))
        return QVariant::fromValue(priv->texture);
    else if (name == QStringLiteral("texture_opacity"))
        return QVariant::fromValue(priv->textureOpacity);
    else if (name == QStringLiteral("isolate"))
        return QVariant::fromValue(priv->isolate);
    else if (name == QStringLiteral("description"))
        return QVariant::fromValue(priv->description);
    else if (name == QStringLiteral("notes"))
        return QVariant::fromValue(priv->notes);
    else if (name == QStringLiteral("size"))
        name = QStringLiteral("radius_logarithmic");
    else if (BOOL_SETTING_NAMES.contains(name))
        isBoolean = true;

    auto iter = priv->settings.find(name);
    if (iter != priv->settings.end())
    {
        if (isMapping)
            return QVariant::fromValue<BrushValueMapping>(iter.value().second);
        else if (isBoolean)
            return QVariant::fromValue<bool>(iter.value().first > 0.0f);
        else
            return QVariant::fromValue<float>(iter.value().first);
    }
    else if (auto settingInfo = getMypaintBrushSettingInfo(name))
    {
        if (isMapping)
            return QVariant::fromValue<BrushValueMapping>({});
        else if (isBoolean)
            return QVariant::fromValue<bool>(settingInfo->def > 0.0f);
        else
            return QVariant::fromValue<float>(settingInfo->def);
    }

    return BaseTool::getToolSetting(name);
}

QList<ToolSettingInfo> MyPaintTool::listToolSettings()
{
    QList<ToolSettingInfo> result;

    result.append(ToolSettingInfo::exponentSlider("size", "SizeExp", -2.0f, 6.0f));
    result.append(ToolSettingInfo::linearSlider("opaque", "Opacity", 0.0f, 1.0f));
    if (priv->maskImages.isEmpty())
        result.append(ToolSettingInfo::linearSlider("hardness", "Hardness", 0.0f, 1.0f));
    result.append(ToolSettingInfo::checkbox("lock_alpha", "Lock Alpha"));
    result.append(ToolSettingInfo::checkbox("eraser", "Erase"));

    return result;
}

QList<ToolSettingInfo> MyPaintTool::listAdvancedSettings()
{
    static const MyPaintBrushSetting settingIdList[] = {
        MYPAINT_BRUSH_SETTING_OPAQUE,
        MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY,
        MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE,
        MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
        MYPAINT_BRUSH_SETTING_HARDNESS,
        MYPAINT_BRUSH_SETTING_ANTI_ALIASING,
        MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS,
        MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS,
        MYPAINT_BRUSH_SETTING_DABS_PER_SECOND,
        MYPAINT_BRUSH_SETTING_RADIUS_BY_RANDOM,
        MYPAINT_BRUSH_SETTING_SPEED1_SLOWNESS,
        MYPAINT_BRUSH_SETTING_SPEED2_SLOWNESS,
        MYPAINT_BRUSH_SETTING_SPEED1_GAMMA,
        MYPAINT_BRUSH_SETTING_SPEED2_GAMMA,
        MYPAINT_BRUSH_SETTING_OFFSET_BY_RANDOM,
        MYPAINT_BRUSH_SETTING_OFFSET_BY_SPEED,
        MYPAINT_BRUSH_SETTING_OFFSET_BY_SPEED_SLOWNESS,
        MYPAINT_BRUSH_SETTING_SLOW_TRACKING,
        MYPAINT_BRUSH_SETTING_SLOW_TRACKING_PER_DAB,
        MYPAINT_BRUSH_SETTING_TRACKING_NOISE,
        //MYPAINT_BRUSH_SETTING_COLOR_H,
        //MYPAINT_BRUSH_SETTING_COLOR_S,
        //MYPAINT_BRUSH_SETTING_COLOR_V,
        MYPAINT_BRUSH_SETTING_RESTORE_COLOR,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S,
        MYPAINT_BRUSH_SETTING_SMUDGE,
        MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH,
        MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG,
        //MYPAINT_BRUSH_SETTING_ERASER,
        MYPAINT_BRUSH_SETTING_STROKE_THRESHOLD,
        MYPAINT_BRUSH_SETTING_STROKE_DURATION_LOGARITHMIC,
        MYPAINT_BRUSH_SETTING_STROKE_HOLDTIME,
        MYPAINT_BRUSH_SETTING_CUSTOM_INPUT,
        MYPAINT_BRUSH_SETTING_CUSTOM_INPUT_SLOWNESS,
        MYPAINT_BRUSH_SETTING_ELLIPTICAL_DAB_RATIO,
        MYPAINT_BRUSH_SETTING_ELLIPTICAL_DAB_ANGLE,
        MYPAINT_BRUSH_SETTING_DIRECTION_FILTER,
        //MYPAINT_BRUSH_SETTING_LOCK_ALPHA,
        //MYPAINT_BRUSH_SETTING_COLORIZE,
        //MYPAINT_BRUSH_SETTING_SNAP_TO_PIXEL,
        MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG
    };

    QList<ToolSettingInfo> result;

    static const QList<QPair<QString, QString>> mappingNames = {
        {"pressure", "Pressure"},
        {"speed1", "Fine speed"},
        {"speed2", "Gross speed"},
        {"random", "Random"},
        {"stroke", "Stroke"},
        {"direction", "Direction"},
        {"direction_angle", "Direction 360"},
        {"tilt_declination", "Declination"},
        {"tilt_ascension", "Ascension"},
        {"custom", "Custom"}
    };

    for(auto settingId: settingIdList)
    {
        auto settingInfo = mypaint_brush_setting_info(settingId);
        auto toolSetting = ToolSettingInfo::linearSlider(settingInfo->cname, settingInfo->name, settingInfo->min, settingInfo->max);
        if (!settingInfo->constant)
            toolSetting.mapping = mappingNames;
        result.append(toolSetting);
    }

    result.append(ToolSettingInfo::maskSet("masks", "Masks"));

    result.append(ToolSettingInfo::texture("texture", "Texture"));

    result.append(ToolSettingInfo::text("description", "Description", false));
    result.append(ToolSettingInfo::text("notes", "Notes", true));

    result.append(ToolSettingInfo::checkbox("isolate", "Isolate Mode"));
    result.append(ToolSettingInfo::linearSlider("texture_opacity", "Texture Opacity", 0.0f, 1.0f));

    return result;
}

QString MyPaintTool::saveExtension()
{
    return QStringLiteral("mbi");
}

QByteArray MyPaintTool::serialize()
{
    QVariantMap document;
    document["version"] = 3;
    document["group"] = "";
    document["comment"] = "";
    document["description"] = priv->description;
    document["notes"] = priv->notes;
    document["parent_brush_name"] = "";

    QVariantMap settingsMap;
    for (auto iter = priv->settings.cbegin(); iter != priv->settings.cend(); ++iter)
    {
        QVariantMap settingMap;
        QVariantMap inputMap;

        settingMap["base_value"] = iter.value().first;

        auto const &mappings = iter.value().second;
        for (auto mapping = mappings.cbegin(); mapping != mappings.cend(); ++mapping)
        {
            QVariantList points;
            for (auto &p: mapping.value())
                points.push_back(QVariantList({p.x(), p.y()}));
            if (!points.isEmpty())
                inputMap[mapping.key()] = points;
        }

        settingMap["inputs"] = inputMap;

        settingsMap[iter.key()] = settingMap;
    }
    document["settings"] = settingsMap;

    QVariantMap imageSettingsMap;
    if (!priv->maskImages.isEmpty())
    {
        QVariantList maskList;
        for (auto const &mask: priv->maskImages)
        {
            MaskBuffer inverted = mask.invert();
            maskList.append(QString(inverted.toPNG().toBase64()));
        }
        imageSettingsMap["masks"] = maskList;
    }

    if (!priv->texture.isNull())
        imageSettingsMap["texture"] = QString(priv->texture.toPNG().toBase64());

    if (priv->textureOpacity != 1.0f)
        imageSettingsMap["texture_opacity"] = priv->textureOpacity;

    if (priv->isolate)
        imageSettingsMap["isolate"] = priv->isolate;

    if (!imageSettingsMap.empty())
        document["image_settings"] = imageSettingsMap;

    return QJsonDocument::fromVariant(QVariant(document)).toJson();
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

std::unique_ptr<StrokeContext> MyPaintTool::newStroke(const StrokeContextArgs &args)
{
    std::unique_ptr<MyPaintStrokeContext> stroke(new MyPaintStrokeContext(args.layer, args.unmodifiedLayer));

    stroke->fromSettings(priv->settings);
    stroke->setMasks(priv->maskImages);
    stroke->setIsolate(priv->isolate);

    if (!priv->texture.isNull())
        stroke->setTexture(priv->texture, priv->textureOpacity);

    return std::move(stroke);
}
