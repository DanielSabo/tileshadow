#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QFileDialog>

namespace FileDialog
{
    QString getSaveFileName(QWidget *parent = 0,
                            const QString &caption = QString(),
                            const QString &dir = QString(),
                            const QString &filter = QString(),
                            QString *selectedFilter = 0,
                            QFileDialog::Options options = 0);

    QString getOpenFileName(QWidget *parent = 0,
                            const QString &caption = QString(),
                            const QString &dir = QString(),
                            const QString &filter = QString(),
                            QString *selectedFilter = 0,
                            QFileDialog::Options options = 0);

    QStringList getOpenFileNames(QWidget *parent = 0,
                                 const QString &caption = QString(),
                                 const QString &dir = QString(),
                                 const QString &filter = QString(),
                                 QString *selectedFilter = 0,
                                 QFileDialog::Options options = 0);
}

#endif // FILEDIALOG_H
