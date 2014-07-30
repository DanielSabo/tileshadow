#ifndef TOOLSETTINGINFO_H
#define TOOLSETTINGINFO_H

#include <QString>

namespace ToolSettingInfoType {
    typedef enum {
        ExponentSlider,
        LinearSlider
    } Type;
}

class ToolSettingInfo
{
public:
    ToolSettingInfoType::Type type;
    QString settingID;
    QString name;
    float min;
    float max;

    static ToolSettingInfo exponentSlider(QString const &settingID, QString const &name, float min, float max);
    static ToolSettingInfo linearSlider(QString const &settingID, QString const &name, float min, float max);
private:
    ToolSettingInfo(ToolSettingInfoType::Type type);
};

#endif // TOOLSETTINGINFO_H
