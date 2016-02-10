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

bool BaseTool::coalesceMovement()
{
    return false;
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

QList<ToolSettingInfo> BaseTool::listAdvancedSettings()
{
    return QList<ToolSettingInfo>();
}

QString BaseTool::saveExtension()
{
    return QString();
}

QByteArray BaseTool::serialize()
{
    return QByteArray();
}
