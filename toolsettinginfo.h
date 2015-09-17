#ifndef TOOLSETTINGINFO_H
#define TOOLSETTINGINFO_H

#include <QString>
#include <QList>
#include <QPair>

namespace ToolSettingInfoType {
    typedef enum {
        ExponentSlider,
        LinearSlider,
        Checkbox,
        Listbox
    } Type;
}

class ToolSettingInfo
{
public:
    typedef QList<QPair<QString, QString>> OptionsList;

    ToolSettingInfoType::Type type;
    QString settingID;
    QString name;
    OptionsList options;
    float min;
    float max;
    QList<QPair<QString, QString>> mapping;

    static ToolSettingInfo exponentSlider(QString const &settingID, QString const &name, float min, float max);
    static ToolSettingInfo linearSlider(QString const &settingID, QString const &name, float min, float max);
    static ToolSettingInfo checkbox(QString const &settingID, QString const &name);
    static ToolSettingInfo listBox(QString const &settingID, QString const &name, OptionsList const &options);
private:
    ToolSettingInfo(ToolSettingInfoType::Type type);
};

#endif // TOOLSETTINGINFO_H
