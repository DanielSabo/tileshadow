#include "toolfactory.h"

#include <QDir>

#include "tiledebugtool.h"
#include "mypainttool.h"

static void asciiTitleCase(QString &instr)
{
    bool lastspace = true;
    for (int i = 0; i < instr.size(); i++)
    {
        if (instr[i].isSpace())
        {
            lastspace = true;
            continue;
        }

        if (lastspace)
        {
            instr[i] = instr[i].toUpper();
            lastspace = false;
        }
    }
}

static QStringList findBrushFiles(QDir const &path)
{
    QFileInfoList infoList = path.entryInfoList();
    QStringList result;

    QFileInfoList directories;

    for (QFileInfo const &info: infoList)
    {
        if (info.isDir())
            directories << info;
        else if (info.fileName().endsWith(".myb"))
            result << info.filePath();
    }

    for (QFileInfo const &info: directories)
    {
        result << findBrushFiles(info.filePath());
    }

    return result;
}

ToolList ToolFactory::listTools()
{
    ToolList toolList;

    toolList.append(QPair<QString, QString>("debug", "Debug"));

    QStringList brushFiles = findBrushFiles(QDir(":/mypaint-tools/"));

    for (int i = 0; i < brushFiles.size(); ++i)
    {
        QString brushFile = brushFiles.at(i);

        QStringList splitName = brushFile.split("/");
        splitName.removeFirst();
        splitName.removeFirst();

        QString buttonName = splitName.join(" - ");
        buttonName.truncate(buttonName.length() - 4);
        buttonName.replace(QChar('_'), " ");
        asciiTitleCase(buttonName);
        toolList.append(QPair<QString, QString>(brushFile, buttonName));
    }

    return toolList;
}

BaseTool *ToolFactory::loadTool(QString toolName)
{
    BaseTool *result = NULL;

    if (toolName.endsWith(".myb"))
    {
        result = new MyPaintTool(toolName);
    }
    else if (toolName == QStringLiteral("debug"))
    {
        result = new TileDebugTool();
    }

    return result;
}

QString ToolFactory::defaultToolName()
{
    return QStringLiteral(":/mypaint-tools/default.myb");
}
