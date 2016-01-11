#include "patternfilltool.h"
#include "toolfactory.h"

#include <QDir>
#include <QStandardPaths>
#include <map>
#include "tiledebugtool.h"
#include "roundbrushtool.h"
#include "mypainttool.h"
#include "gradienttool.h"
#include <QDebug>

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
    QFileInfoList infoList = path.entryInfoList({"*.myb", "*.mbi"}, QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);
    QStringList result;

    QFileInfoList directories;

    for (QFileInfo const &info: infoList)
    {
        if (info.isDir())
            directories << info;
        else
            result << info.filePath();
    }

    for (QFileInfo const &info: directories)
    {
        result << findBrushFiles(info.filePath());
    }

    return result;
}

namespace {
    struct SortKey
    {
        QString filename;
        QString dirname;
        QString displayName;

        SortKey(QString path)
        {
            path.chop(4);
            path.replace(QChar('_'), " ");
            QStringList splitName = path.split("/");
            filename = splitName.takeLast();
            dirname = splitName.join(" - ");
            if (!dirname.isEmpty())
                displayName = dirname + " - " + filename;
            else
                displayName = filename;
            asciiTitleCase(displayName);
        }

        bool operator<(const SortKey &b) const
        {
            int cmp = QString::compare(dirname, b.dirname, Qt::CaseInsensitive);
            if (cmp < 0)
                return true;
            else if (cmp > 0)
                return false;

            if (QString::compare(filename, b.filename, Qt::CaseInsensitive) < 0)
                return true;
            return false;
        }
    };
}

static void addFilesInPath(std::map<SortKey, QString> &index, QString const &path)
{
    const int prefixLength = path.length();

    for (QString const &brushFile: findBrushFiles(QDir(path)))
    {
        QString name = brushFile;
        name.remove(0, prefixLength);
        index[name] = brushFile;
    }
}

QString ToolFactory::getUserToolsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/mypaint-tools/");
}

ToolList ToolFactory::listTools()
{
    ToolList toolList;

    toolList.append(QPair<QString, QString>("debug", "Debug"));
    toolList.append(QPair<QString, QString>("round", "Round"));
    toolList.append(QPair<QString, QString>("gradient", "Apply Gradient"));
    toolList.append(QPair<QString, QString>("pattern-fill", "Pattern Fill"));

    std::map<SortKey, QString> items;

    addFilesInPath(items, QStringLiteral(":/mypaint-tools/"));
    addFilesInPath(items, ToolFactory::getUserToolsPath());

    for (auto const &iter: items)
    {
        QString const &brushFile = iter.second;
        QString const &brushName = iter.first.displayName;
        toolList.append(QPair<QString, QString>(brushFile, brushName));
    }

    return toolList;
}

BaseTool *ToolFactory::loadTool(QString toolName)
{
    BaseTool *result = NULL;

    if (toolName.endsWith(".myb") ||
        toolName.endsWith(".mbi"))
    {
        result = new MyPaintTool(toolName);
    }
    else if (toolName == QStringLiteral("debug"))
    {
        result = new TileDebugTool();
    }
    else if (toolName == QStringLiteral("round"))
    {
        result = new RoundBrushTool();
    }
    else if (toolName == QStringLiteral("gradient"))
    {
        result = new GradientTool();
    }
    else if (toolName == QStringLiteral("pattern-fill"))
    {
        QStringList directories = {
            QStringLiteral(":/patterns/"),
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/patterns/")
        };
        result = new PatternFillTool(directories);
    }

    return result;
}

QString ToolFactory::defaultToolName()
{
    return QStringLiteral(":/mypaint-tools/deevad/brush.myb");
}

QString ToolFactory::defaultEraserName()
{
    return QStringLiteral(":/mypaint-tools/deevad/thin_hard_eraser.myb");
}

QString ToolFactory::savePathForExtension(QString extension)
{
    if (extension == QStringLiteral("myb") ||
        extension == QStringLiteral("mbi"))
    {
        return ToolFactory::getUserToolsPath();
    }

    return QString();
}
