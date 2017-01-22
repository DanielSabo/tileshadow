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

ToolSettingInfo ToolSettingInfo::checkbox(const QString &settingID, const QString &name)
{
    ToolSettingInfo result(ToolSettingInfoType::Checkbox);
    result.settingID = settingID;
    result.name = name;
    result.min = 0;
    result.max = 1;

    return result;
}

ToolSettingInfo ToolSettingInfo::listBox(const QString &settingID, const QString &name, const OptionsList &options)
{
    ToolSettingInfo result(ToolSettingInfoType::Listbox);
    result.settingID = settingID;
    result.name = name;
    result.options = options;

    return result;
}

ToolSettingInfo ToolSettingInfo::maskSet(const QString &settingID, const QString &name)
{
    ToolSettingInfo result(ToolSettingInfoType::MaskSet);
    result.settingID = settingID;
    result.name = name;
    return result;
}

ToolSettingInfo ToolSettingInfo::texture(const QString &settingID, const QString &name)
{
    ToolSettingInfo result(ToolSettingInfoType::Texture);
    result.settingID = settingID;
    result.name = name;
    return result;
}

ToolSettingInfo::ToolSettingInfo(ToolSettingInfoType::Type type)
    : type(type)
{
}
