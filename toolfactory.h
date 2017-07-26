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
    ToolList listPalettes();

    std::unique_ptr<BaseTool> loadTool(QString toolName);
    void initializeUserPaths();

    QString getUserToolsPath();
    QString getUserPatternPath();
    QString getUserPalettePath();

    QString defaultToolName();
    QString defaultEraserName();
    QString defaultPaletteName();

    QString savePathForExtension(QString extension);
}

#endif // TOOLFACTORY_H
