#include "toolsettinginfo.h"

ToolSettingInfo ToolSettingInfo::exponentSlider(const QString &settingID, const QString &name, float min, float max)
{
    ToolSettingInfo result(ToolSettingInfoType::ExponentSlider);
    result.settingID = settingID;
    result.name = name;
    result.min = min;
    result.max = max;

    return result;
}

ToolSettingInfo ToolSettingInfo::linearSlider(const QString &settingID, const QString &name, float min, float max)
{
    ToolSettingInfo result(ToolSettingInfoType::LinearSlider);
    result.settingID = settingID;
    result.name = name;
    result.min = min;
    result.max = max;

    return result;
}

ToolSettingInfo::ToolSettingInfo(ToolSettingInfoType::Type type)
    : type(type)
{
}
