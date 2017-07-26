#ifndef PALETTEVIEW_H
#define PALETTEVIEW_H

#include "dragscrollarea.h"

//FIXME: Define PaletteEntry somewhere common
#include "colorpalette.h"

#include <QMenu>

class PaletteView : public DragScrollArea
{
    Q_OBJECT
public:
    PaletteView(QWidget *parent = 0);
    ~PaletteView();

    QSize rowSize() const override;
    int rowCount() const override;
    QSize viewportSizeHint() const override;

    void setColorList(ColorPalette const &colors);
    void setCurrentColor(QColor const &color);
    ColorPalette getColorList();
    void setModified(bool state);
    bool isModified();

signals:
    void colorHover(QColor c);
    void colorClick(QColor c);
    void colorSelect(QColor c);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void viewportClicked(QMouseEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    void hoverAt(QPoint p);
    int colorAt(QPoint p);

    QMenu *contextMenu;
    QAction *contextMenu_Add;
    QAction *contextMenu_Remove;
    QAction *contextMenu_Select;
    QColor currentColor;
    std::vector<PaletteEntry> colorList;
    int hoverIdx;
    int perRow;
    bool modified;
};

#endif // PALETTEVIEW_H
