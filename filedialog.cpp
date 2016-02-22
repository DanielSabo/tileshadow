#include "filedialog.h"

#ifdef Q_OS_LINUX
// Disable native file dialogs due to KDE Bug 357684
#define ADDITIONAL_OPITONS QFileDialog::DontUseNativeDialog;
#else
#define ADDITIONAL_OPITONS QFileDialog::Option();
#endif

QString FileDialog::getSaveFileName(QWidget              *parent,
                                    const QString        &caption,
                                    const QString        &dir,
                                    const QString        &filter,
                                    QString              *selectedFilter,
                                    QFileDialog::Options  options)
{
    options |= ADDITIONAL_OPITONS;
    return QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter, options);
}

QString FileDialog::getOpenFileName(QWidget              *parent,
                                    const QString        &caption,
                                    const QString        &dir,
                                    const QString        &filter,
                                    QString              *selectedFilter,
                                    QFileDialog::Options  options)
{
    options |= ADDITIONAL_OPITONS;
    return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options);
}

QStringList FileDialog::getOpenFileNames(QWidget              *parent,
                                         const QString        &caption,
                                         const QString        &dir,
                                         const QString        &filter,
                                         QString              *selectedFilter,
                                         QFileDialog::Options  options)
{
    options |= ADDITIONAL_OPITONS;
    return QFileDialog::getOpenFileNames(parent, caption, dir, filter, selectedFilter, options);
}
