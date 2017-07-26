#include "colorpalette.h"
#include <QRegularExpression>
#include <QDebug>

ColorPalette readGPL(QIODevice *file)
{
    if (!file->isOpen())
        file->open(QIODevice::ReadOnly);

    ColorPalette result;
    QString data = QString::fromUtf8(file->readAll());
    auto lines = data.splitRef("\n");
    auto lineIter = lines.begin();
    if (!lineIter->startsWith("GIMP Palette"))
        return {};
//    qDebug() << *lineIter;

    QRegularExpression paletteTagRegExp("^([^:]+):\\s*(.*)");
    while (lineIter != lines.end() && !lineIter->startsWith("#"))
    {
        auto match = paletteTagRegExp.match(*lineIter);
        if (match.hasMatch())
        {
            if (match.capturedRef(1) == QStringLiteral("Name"))
                result.name = match.captured(2);
            else if (match.capturedRef(1) == QStringLiteral("Columns"))
                result.columns = match.capturedRef(2).toInt();
        }

        lineIter++;
    }

//    qDebug() << "Name" << result.name;
//    qDebug() << "Columns" << result.columns;

    while (lineIter != lines.end() && lineIter->startsWith("#"))
        lineIter++;

    if (lineIter == lines.end())
        return {};
//    qDebug() << "Skipped" << (lineIter - lines.begin());

    QRegularExpression colorEntryRegExp("^\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*(.*)");
    colorEntryRegExp.optimize();

    while (lineIter != lines.end())
    {
        if (!lineIter->startsWith("#"))
        {
            auto match = colorEntryRegExp.match(*lineIter);
            if (match.hasMatch())
            {
//                qDebug() << match;
                result.values.emplace_back(PaletteEntry{match.captured(4), QColor(match.capturedRef(1).toInt(), match.capturedRef(2).toInt(), match.capturedRef(3).toInt())});
            }
        }

        lineIter++;
    }

//    qDebug() << lines.size();
//    qDebug() << result.size();

    return result;
}

QByteArray ColorPalette::toGPL()
{
    QByteArray result;
    result.append(QStringLiteral("GIMP Palette\n"));
    if (!name.isEmpty())
        result.append(QStringLiteral("Name: %1\n").arg(name));
    if (columns != 0)
        result.append(QStringLiteral("Columns: %1\n").arg(columns));
    result.append("#\n");

    for (const auto &c: values)
    {
        QString entry = QStringLiteral("%1 %2 %3 %4\n").arg(c.color.red(), 3).arg(c.color.green(), 3).arg(c.color.blue(), 3).arg(c.name);
        result.append(entry);
    }

    return result;
}
