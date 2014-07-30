#include "basetool.h"
#include <QDebug>

BaseTool::BaseTool()
{
}

BaseTool::~BaseTool()
{
}

void BaseTool::setColor(const QColor &color)
{
}

void BaseTool::setToolSetting(QString const &name, QVariant const &value)
{
    qDebug() << "unknown tool setting" << name << ":" << value;
}

QVariant BaseTool::getToolSetting(const QString &name)
{
    qDebug() << "unknown tool setting" << name;

    return QVariant();
}

QList<ToolSettingInfo> BaseTool::listToolSettings()
{
    return QList<ToolSettingInfo>();
}
