#ifndef TOOLFACTORY_H
#define TOOLFACTORY_H

#include <QList>
#include <QPair>
#include <QString>
#include <memory>
#include "basetool.h"

typedef QList<QPair<QString, QString> > ToolList;

namespace ToolFactory {
    ToolList listTools();
    std::unique_ptr<BaseTool> loadTool(QString toolName);
    void initializeUserPaths();
    QString getUserToolsPath();
    QString getUserPatternPath();
    QString defaultToolName();
    QString defaultEraserName();
    QString savePathForExtension(QString extension);
}

#endif // TOOLFACTORY_H
