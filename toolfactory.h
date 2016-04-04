#ifndef TOOLFACTORY_H
#define TOOLFACTORY_H

#include <QList>
#include <QPair>
#include <QString>
#include "basetool.h"

typedef QList<QPair<QString, QString> > ToolList;

namespace ToolFactory {
    ToolList listTools();
    BaseTool *loadTool(QString toolName);
    void initializeUserPaths();
    QString getUserToolsPath();
    QString defaultToolName();
    QString defaultEraserName();
    QString savePathForExtension(QString extension);
}

#endif // TOOLFACTORY_H
