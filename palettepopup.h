#ifndef PALETTEPOPUP_H
#define PALETTEPOPUP_H
#include "sidebarpopup.h"
#include "colorpalette.h"

class PalettePopupPrivate;
class PalettePopup : public SidebarPopup
{
    Q_OBJECT
public:
    PalettePopup(QWidget *parent);
    ~PalettePopup();

    void setColorList(QString const &path, ColorPalette const &colors);
    void setCurrentColor(QColor const &color);

    void promptNew();
    void promptSaveAs();

protected:
    void showEvent(QShowEvent *event);

    bool promptIfModified();

signals:
    void colorHover(const QColor &color);
    void colorSelect(const QColor &color);
    void promptPalette();

private:
    QScopedPointer<PalettePopupPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(PalettePopup)
};

#endif // PALETTEPOPUP_H
