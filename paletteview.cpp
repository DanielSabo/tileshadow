#include "paletteview.h"
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QCursor>

namespace  {
const int defaultPerRow = 10;
const int spacing = 2;
const int rectWidth = 16;
}

PaletteView::PaletteView(QWidget *parent)
    : DragScrollArea(parent),
      contextMenu(new QMenu(this)),
      hoverIdx(-1),
      perRow(defaultPerRow),
      modified(false)
{
    setAutoFillBackground(false);
    setMouseTracking(true);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, [this] (int value) {
        QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
        hoverAt(pos);
    });

    contextMenu_Add = contextMenu->addAction("Add Color");
    contextMenu_Remove = contextMenu->addAction("Remove Color");
    contextMenu_Select = contextMenu->addAction("Select Color");
}

PaletteView::~PaletteView()
{
}

void PaletteView::viewportClicked(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    int idx = colorAt(event->pos());
    if (idx != -1)
        colorClick(colorList.at(idx).color);
}

QSize PaletteView::rowSize() const
{
    int w = perRow * (rectWidth + spacing) + spacing;
    int h = rectWidth + spacing;

    return QSize(w, h);
}

int PaletteView::rowCount() const
{
    return qMax(1, ((int)colorList.size() + perRow - 1) / perRow);
}

QSize PaletteView::viewportSizeHint() const
{
    QSize row = rowSize();

    return QSize(row.width(), row.height() * rowCount() + spacing);
}

void PaletteView::setColorList(ColorPalette const &colors)
{
    modified = false;
    colorList = colors.values;
    perRow = colors.columns ? colors.columns : defaultPerRow;
    updateGeometry();
    viewport()->update();
}

void PaletteView::setCurrentColor(const QColor &color)
{
    currentColor = color;
}

ColorPalette PaletteView::getColorList()
{
    ColorPalette result;
    result.columns = perRow;
    result.values = colorList;
    return result;
}

void PaletteView::setModified(bool state)
{
    modified = state;
}

bool PaletteView::isModified()
{
    return modified;
}

void PaletteView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), QColor(128, 128, 128));
    const int scrollOffset = verticalScrollBar()->value();

    QRect swabRect = {spacing, spacing - scrollOffset, rectWidth, rectWidth};
    for (auto const &c: colorList)
    {
        if (c.color == QColor(128, 128, 128))
        {
            QRect border = swabRect.adjusted(-1, -1, 1, 1);
            painter.fillRect(border, Qt::black);
        }

        painter.fillRect(swabRect, c.color);
        swabRect = swabRect.translated(spacing + rectWidth, 0);
        if (swabRect.right() + spacing >= viewport()->width())
            swabRect = {spacing, swabRect.y() + rectWidth + spacing, rectWidth, rectWidth};
    }
}

void PaletteView::mouseMoveEvent(QMouseEvent *event)
{
    if (event->button() == Qt::NoButton)
        hoverAt(event->pos());

    DragScrollArea::mouseMoveEvent(event);
}

void PaletteView::contextMenuEvent(QContextMenuEvent *event)
{
    int idx = colorAt(viewport()->mapFromGlobal(event->globalPos()));
    contextMenu_Select->setEnabled(idx != -1);
    contextMenu_Remove->setEnabled(idx != -1);
    QAction *selected = contextMenu->exec(event->globalPos());
    if (selected == contextMenu_Add)
    {
        modified = true;
        if (idx != -1)
            colorList.insert(colorList.begin() + idx, {QString(), currentColor});
        else
            colorList.insert(colorList.end(), {QString(), currentColor});
//        viewport()->updateGeometry();
        viewport()->update();
    }
    else if (selected == contextMenu_Select)
    {
        colorSelect(colorList.at(idx).color);
    }
    else if (selected == contextMenu_Remove)
    {
        modified = true;
        colorList.erase(colorList.begin() + idx);
//        viewport()->updateGeometry();
        viewport()->update();
    }
}

bool PaletteView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Leave)
    {
        if (hoverIdx != -1)
            colorHover(QColor());
        hoverIdx = -1;
    }
    return DragScrollArea::viewportEvent(event);
}

void PaletteView::hoverAt(QPoint p)
{
    int idx = colorAt(p);
    if (idx != hoverIdx)
    {
        if (idx != -1)
            colorHover(colorList.at(idx).color);
        else
            colorHover(QColor());
        hoverIdx = idx;
    }
}

int PaletteView::colorAt(QPoint p)
{
    const int swabsPerRow = viewport()->width() / (spacing + rectWidth);

    int row = (p.y() + verticalScrollBar()->value()) / (spacing + rectWidth);
    int col = qMax(0, (p.x() - spacing) / (spacing + rectWidth));

    int idx = swabsPerRow * row + col;
    if (idx < (int)colorList.size())
        return idx;
    return -1;
}
