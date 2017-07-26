#include "palettepopup.h"
#include "paletteview.h"
#include "toolfactory.h"
#include "filedialog.h"

#include <QDir>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMenu>
#include <QSaveFile>
#include <QMessageBox>

class PalettePopupPrivate {
public:
    QString currentPath;
    PaletteView *paletteView = nullptr;
    QMenu *menu = nullptr;
    QAction *menuNew = nullptr;
    QAction *menuSaveAs = nullptr;
};

PalettePopup::PalettePopup(QWidget *parent) :
    SidebarPopup(parent),
    d_ptr(new PalettePopupPrivate())
{
    Q_D(PalettePopup);

    QVBoxLayout *palettePopupLayout = new QVBoxLayout(this);
    palettePopupLayout->setSpacing(3);
    palettePopupLayout->setContentsMargins(1, 1, 1, 1);
    d->paletteView = new PaletteView(this);
    d->paletteView->setMinimumSize(d->paletteView->rowSize());

    connect(d->paletteView, &PaletteView::colorHover, this, &PalettePopup::colorHover);
    connect(d->paletteView, &PaletteView::colorClick, this, [this] (const QColor &color) {
        hide();
        colorSelect(color);
    });
    connect(d->paletteView, &PaletteView::colorSelect, this, &PalettePopup::colorSelect);

    QHBoxLayout *palettePopupButtons = new QHBoxLayout();
    palettePopupLayout->setSpacing(0);
    palettePopupLayout->setContentsMargins(0, 0, 0, 0);

    QPushButton *paletteSelect = new QPushButton(tr("Select Palette"));

    connect(paletteSelect, &QPushButton::clicked, this, [this] {
        if (promptIfModified())
            promptPalette();
    });

    QPushButton *paletteMenu = new QPushButton(tr("..."));
    d->menu = new QMenu(this);
    d->menuNew = d->menu->addAction(tr("New Palette"));
    d->menuSaveAs = d->menu->addAction(tr("Save Palette As..."));
    paletteMenu->setMenu(d->menu);

    connect(d->menuNew, &QAction::triggered, this, &PalettePopup::promptNew);
    connect(d->menuSaveAs, &QAction::triggered, this, &PalettePopup::promptSaveAs);

    palettePopupButtons->addWidget(paletteSelect);
    palettePopupButtons->addWidget(paletteMenu);
    palettePopupButtons->setStretch(0, 1);
    palettePopupLayout->addLayout(palettePopupButtons);
    palettePopupLayout->addWidget(d->paletteView);
}

PalettePopup::~PalettePopup()
{

}

void PalettePopup::setColorList(const QString &path, const ColorPalette &colors)
{
    Q_D(PalettePopup);

    d->currentPath = path;
    d->paletteView->setColorList(colors);
    d->paletteView->setFocus();
}

void PalettePopup::setCurrentColor(const QColor &color)
{
    Q_D(PalettePopup);

    d->paletteView->setCurrentColor(color);
}

void PalettePopup::promptNew()
{
    Q_D(PalettePopup);

    if (!promptIfModified())
        return;

    d->currentPath = QString();
    d->paletteView->setColorList({});
    d->paletteView->setFocus();
}

void PalettePopup::promptSaveAs()
{
    Q_D(PalettePopup);

    QString defaultFilename;
    if (d->currentPath.startsWith(":/") || d->currentPath.isEmpty())
    {
        defaultFilename = ToolFactory::getUserPalettePath() + QDir::toNativeSeparators("/") +
                          QObject::tr("untitled") + ".gpl";
    }
    else
    {
        defaultFilename = d->currentPath;
    }

    QString filename = FileDialog::getSaveFileName(nullptr, QObject::tr("Save As..."),
                                                   defaultFilename,
                                                   QObject::tr("GIMP Color Palette") + " (*.gpl)");

    if (filename.isEmpty())
        return;

    QSaveFile outfile(filename);
    outfile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    outfile.write(d->paletteView->getColorList().toGPL());
    outfile.commit();

    d->currentPath = filename;
    d->paletteView->setModified(false);
}

void PalettePopup::showEvent(QShowEvent *)
{
    Q_D(PalettePopup);

    d->paletteView->setFocus();
}

bool PalettePopup::promptIfModified()
{
    Q_D(PalettePopup);

    if (d->paletteView->isModified())
    {
        QMessageBox savePrompt(this);
        savePrompt.setIcon(QMessageBox::Warning);
        savePrompt.setText(tr("The current palette has unsaved changes, are you sure you want to discard it?"));
        savePrompt.setStandardButtons(QMessageBox::Discard | QMessageBox::Cancel);
        savePrompt.setDefaultButton(QMessageBox::Cancel);

        int result = savePrompt.exec();
        if (result != QMessageBox::Discard)
            return false;
    }

    return true;
}
