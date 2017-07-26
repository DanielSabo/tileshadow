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
#include <QSettings>

namespace {
void asciiTitleCase(QString &instr)
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

QStringList findBrushFiles(QDir const &path, QStringList const &filter)
{
    QFileInfoList infoList = path.entryInfoList(filter, QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);
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
        result << findBrushFiles(info.filePath(), filter);
    }

    return result;
}

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
        if (dirname.isEmpty())
        {
            if (!b.dirname.isEmpty())
                return true;
        }
        else if (b.dirname.isEmpty())
            return false;

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

void addFilesInPath(std::map<SortKey, QString> &index, QString const &path, QStringList const &filter)
{
    int prefixLength = path.length();

    if (!path.endsWith(QDir::separator()))
        prefixLength += 1;

    for (QString const &brushFile: findBrushFiles(QDir(path), filter))
    {
        QString name = brushFile;
        name.remove(0, prefixLength);
        index[name] = brushFile;
    }
}
}

void ToolFactory::initializeUserPaths()
{
    QDir prefix{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)};
    prefix.mkpath(QStringLiteral("mypaint-tools"));
    prefix.mkpath(QStringLiteral("patterns"));
    prefix.mkpath(QStringLiteral("palettes"));
}

QString ToolFactory::getUserToolsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/mypaint-tools/");
}

QString ToolFactory::getUserPatternPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/patterns/");
}

QString ToolFactory::getUserPalettePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/palettes/");
}

ToolList ToolFactory::listTools()
{
    ToolList toolList;

    toolList.append(QPair<QString, QString>("debug", "Debug"));
    toolList.append(QPair<QString, QString>("round", "Round"));
    toolList.append(QPair<QString, QString>("gradient", "Apply Gradient"));
    toolList.append(QPair<QString, QString>("pattern-fill", "Pattern Fill"));

    QStringList filter = {"*.myb", "*.mbi"};
    std::map<SortKey, QString> items;

    addFilesInPath(items, QStringLiteral(":/mypaint-tools/"), filter);
    addFilesInPath(items, ToolFactory::getUserToolsPath(), filter);

    for (QString path: QSettings().value("Paths/UserTools").toStringList())
        addFilesInPath(items, path, filter);

    for (auto const &iter: items)
    {
        QString const &brushFile = iter.second;
        QString const &brushName = iter.first.displayName;
        toolList.append(QPair<QString, QString>(brushFile, brushName));
    }

    return toolList;
}

ToolList ToolFactory::listPalettes()
{
    ToolList paletteList;
    std::map<SortKey, QString> items;

    QStringList filter = {"*.gpl"};
    addFilesInPath(items, QStringLiteral(":/palettes/"), filter);
    addFilesInPath(items, ToolFactory::getUserPalettePath(), filter);

    for (QString path: QSettings().value("Paths/UserPalettes").toStringList())
        addFilesInPath(items, path, filter);

    for (auto const &iter: items)
    {
        QString const &filename = iter.second;
        QString const &name = iter.first.displayName;
        paletteList.append(QPair<QString, QString>(filename, name));
    }

    return paletteList;
}

std::unique_ptr<BaseTool> ToolFactory::loadTool(QString toolName)
{
    BaseTool *result = nullptr;

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
            ToolFactory::getUserPatternPath()
        };

        directories << QSettings().value("Paths/UserPatterns").toStringList();

        result = new PatternFillTool(directories);
    }

    return std::unique_ptr<BaseTool>(result);
}

QString ToolFactory::defaultToolName()
{
    return QStringLiteral(":/mypaint-tools/deevad/brush.myb");
}

QString ToolFactory::defaultEraserName()
{
    return QStringLiteral(":/mypaint-tools/deevad/thin_hard_eraser.myb");
}

QString ToolFactory::defaultPaletteName()
{
    return QStringLiteral(":/palettes/Named_Colors.gpl");
}

QString ToolFactory::savePathForExtension(QString extension)
{
    if (extension == QStringLiteral("myb") ||
        extension == QStringLiteral("mbi"))
    {
        return ToolFactory::getUserToolsPath();
    }
    else if (extension == QStringLiteral("gpl"))
    {
        return ToolFactory::getUserPalettePath();
    }

    return QString();
}
