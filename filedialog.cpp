#include "filedialog.h"
#include <memory>
#include <QSettings>
#include <QMimeDatabase>
#include <QFileIconProvider>
#include <QFileInfo>

namespace {
#ifdef Q_OS_LINUX
// Workaround for KDE Bug 358926
class IconProvider : public QFileIconProvider
{
    QIcon icon(const QFileInfo &info) const {
        auto mimeType = QMimeDatabase().mimeTypeForFile(info);
        return QIcon::fromTheme(mimeType.iconName());
    }
};
#endif

QStringList showFileDialog(QWidget                 *parent,
                           const QString           &caption,
                           const QString           &dir,
                           const QString           &filter,
                           QFileDialog::Options     options,
                           QFileDialog::AcceptMode  acceptMode,
                           QFileDialog::FileMode    fileMode)
{
    QFileDialog dialog{parent, caption, dir, filter};
    dialog.setAcceptMode(acceptMode);
    dialog.setFileMode(fileMode);
#ifdef Q_OS_LINUX
    // Disable native file dialogs due to KDE Bug 357684
    dialog.setOptions(options | QFileDialog::DontUseNativeDialog);
    IconProvider iconProvider;
    dialog.setIconProvider(&iconProvider);

    QSettings settings;
    QByteArray geometry = settings.value("FileDialog/geometry").toByteArray();
    if (!geometry.isEmpty())
        dialog.restoreGeometry(geometry);
#else
    dialog.setOptions(options);
#endif

    QStringList result;
    if (dialog.exec() == QDialog::Accepted)
        result = dialog.selectedFiles();

#ifdef Q_OS_LINUX
    settings.setValue("FileDialog/geometry", dialog.saveGeometry());
#endif

    return result;
}
}


QString FileDialog::getSaveFileName(QWidget              *parent,
                                    const QString        &caption,
                                    const QString        &dir,
                                    const QString        &filter,
                                    QString              *selectedFilter,
                                    QFileDialog::Options  options)
{
    return showFileDialog(parent, caption, dir, filter, options, QFileDialog::AcceptSave, QFileDialog::AnyFile).value(0);
}

QString FileDialog::getOpenFileName(QWidget              *parent,
                                    const QString        &caption,
                                    const QString        &dir,
                                    const QString        &filter,
                                    QString              *selectedFilter,
                                    QFileDialog::Options  options)
{
    return showFileDialog(parent, caption, dir, filter, options, QFileDialog::AcceptOpen, QFileDialog::ExistingFile).value(0);
}

QStringList FileDialog::getOpenFileNames(QWidget              *parent,
                                         const QString        &caption,
                                         const QString        &dir,
                                         const QString        &filter,
                                         QString              *selectedFilter,
                                         QFileDialog::Options  options)
{
    return showFileDialog(parent, caption, dir, filter, options, QFileDialog::AcceptOpen, QFileDialog::ExistingFiles);
}

QString FileDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir, QFileDialog::Options options)
{
    return showFileDialog(parent, caption, dir, QString(), options, QFileDialog::AcceptOpen, QFileDialog::Directory).value(0);
}
