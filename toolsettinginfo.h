#ifndef TOOLSETTINGINFO_H
#define TOOLSETTINGINFO_H

#include <QString>

namespace ToolSettingInfoType {
    typedef enum {
        ExponentSlider,
        LinearSlider,
        Checkbox
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
    static ToolSettingInfo checkbox(QString const &settingID, QString const &name);
private:
    ToolSettingInfo(ToolSettingInfoType::Type type);
};

#endif // TOOLSETTINGINFO_H
