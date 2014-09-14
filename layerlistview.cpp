#include "layerlistview.h"
#include <QStyle>
#include <QStyleOption>
#include <QStylePainter>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QApplication>
#include <QLineEdit>
#include <algorithm>

class LayerListViewPrivate
{
public:
    int vPad;
    int hPad;
    int iconSize;

    bool hasFocus;
    int focusRow;
    int focusColumn;

    QSize rowSize;

    QLineEdit *nameEditor;

    int clickedColumn(int x) {
        if (x <= (hPad * 2 + iconSize))
            return LayerListView::VisibleColumn;
        else if (x <= hPad * 3 + iconSize * 2)
            return LayerListView::EditableColumn;
        else
            return LayerListView::NameColumn;
    }
};

LayerListView::LayerListView(QWidget *parent) :
    QWidget(parent),
    d_ptr(new LayerListViewPrivate)
{
    Q_D(LayerListView);

    setPalette(QApplication::palette("QAbstractItemView"));
    setFont(QApplication::font("QAbstractItemView"));
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    visibleIcon = QIcon(":/icons/tileshadow-visible.png");
    visibleIcon.addFile(":/icons/tileshadow-visible.png", QSize(), QIcon::Normal, QIcon::On);
    visibleIcon.addFile(":/icons/tileshadow-visible-off.png", QSize(), QIcon::Normal, QIcon::Off);
    lockedIcon = QIcon(":/icons/tileshadow-lock.png");
    lockedIcon.addFile(":/icons/tileshadow-lock.png", QSize(), QIcon::Normal, QIcon::On);
    lockedIcon.addFile(":/icons/tileshadow-lock-off.png", QSize(), QIcon::Normal, QIcon::Off);

    d->hPad = 2;
    d->vPad = 1;
    d->iconSize = 16;
    d->hasFocus = false;

    d->nameEditor = new QLineEdit(this);
    d->nameEditor->installEventFilter(this);
    d->nameEditor->hide();

    int rowHeight = d->nameEditor->sizeHint().height();
        rowHeight = qMax(rowHeight, fontMetrics().height() + d->vPad * 2);
        rowHeight = qMax(rowHeight, d->iconSize + d->vPad * 2);

    d->rowSize = QSize(100, rowHeight);

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QSize LayerListView::sizeHint() const
{
    Q_D(const LayerListView);

    return QSize(d->rowSize.width(),
                 d->rowSize.height() * qMax<int>(1, things.size()));
}

void LayerListView::setData(const QList<CanvasWidget::LayerInfo> &data, int selection)
{
    Q_D(LayerListView);

    if (selection < 0 || selection >= data.size())
        selection = 0;

    d->nameEditor->hide();

    things = data;
    std::reverse(things.begin(), things.end());
    selectedThing = rowFixup(selection);

    recalulateSize();
    update();
}

int LayerListView::getSelectedRow()
{
    return rowFixup(selectedThing);
}

void LayerListView::paintEvent(QPaintEvent *event)
{
    Q_D(LayerListView);

    QPainter painter(this);

    QSize widgetRowSize(width(), d->rowSize.height());

    for (int i = 0; i < things.size(); ++i)
    {
        CanvasWidget::LayerInfo const &thing = things.at(i);

        QPoint rowOrigin = QPoint(0, d->rowSize.height() * i);
        QPoint contentOrigin = rowOrigin + QPoint(d->hPad, d->vPad);

        if (d->hasFocus && i == d->focusRow && d->focusColumn == NameColumn)
            painter.fillRect(QRect(rowOrigin, widgetRowSize), palette().brush(QPalette::Normal, QPalette::Highlight));
        else if (i == selectedThing)
            painter.fillRect(QRect(rowOrigin, widgetRowSize), palette().brush(QPalette::Inactive, QPalette::Highlight));

        QRect visibleIconRect = QRect(contentOrigin, QSize(d->iconSize, d->iconSize));
        if (d->rowSize.height() > d->iconSize)
            visibleIconRect.translate(0, (d->rowSize.height() - d->iconSize) / 2);
        QRect lockIconRect = visibleIconRect.translated(visibleIconRect.width() + d->hPad, 0);

        if (d->hasFocus && i == d->focusRow)
        {
            if (d->focusColumn == VisibleColumn)
            {
                QRect focusRect = QRect(rowOrigin.x(),
                                        rowOrigin.y(),
                                        d->iconSize + d->hPad * 2,
                                        d->rowSize.height());
                painter.fillRect(focusRect, palette().brush(QPalette::Normal, QPalette::Highlight));
            }
            else if (d->focusColumn == EditableColumn)
            {
                QRect focusRect = QRect(rowOrigin.x() + d->iconSize + d->hPad,
                                        rowOrigin.y(),
                                        d->iconSize + d->hPad * 2,
                                        d->rowSize.height());
                painter.fillRect(focusRect, palette().brush(QPalette::Normal, QPalette::Highlight));
            }
        }

        if (thing.visible)
            visibleIcon.paint(&painter, visibleIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
        else
            visibleIcon.paint(&painter, visibleIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        if (!thing.editable)
            lockedIcon.paint(&painter, lockIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
        else
            lockedIcon.paint(&painter, lockIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        QPoint textOrigin = QPoint(lockIconRect.x() + visibleIconRect.width() + d->hPad,
                                   contentOrigin.y());
        QRect textRect = QRect(textOrigin,
                               QSize(widgetRowSize.width() - textOrigin.x() - d->hPad,
                                     widgetRowSize.height() - d->vPad * 2));

        painter.save();
        if (d->hasFocus && i == d->focusRow && d->focusColumn == NameColumn)
            painter.setPen(palette().color(QPalette::Normal, QPalette::HighlightedText));
        else if (i == selectedThing)
            painter.setPen(palette().color(QPalette::Inactive, QPalette::HighlightedText));
        else
            painter.setPen(palette().color(QPalette::Normal, QPalette::Text));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, thing.name);
        painter.restore();
    }
}

void LayerListView::mousePressEvent(QMouseEvent *event)
{
    Q_D(LayerListView);

    if (event->button() != Qt::LeftButton)
        return;
    if (d->nameEditor->isVisible())
        return;

    int clickedRow = qMax(qMin(event->y() / d->rowSize.height(), things.size() - 1), 0);
    int clickedColumn = d->clickedColumn(event->x());

    if (event->type() == QEvent::MouseButtonDblClick &&
        clickedColumn == NameColumn)
    {
        selectedThing = clickedRow;
        d->nameEditor->setText(things[clickedRow].name);
        d->nameEditor->selectAll();
        int leadingWidth = d->iconSize * 2 + d->vPad * 2;
        d->nameEditor->move(d->iconSize * 2 + d->vPad * 2,
                            d->rowSize.height() * selectedThing);
        d->nameEditor->setMaximumWidth(width() - leadingWidth - d->vPad);
        d->nameEditor->show();
        d->nameEditor->setFocus(Qt::MouseFocusReason);
    }
    else
    {
        d->hasFocus = true;
        d->focusRow = clickedRow;
        d->focusColumn = d->clickedColumn(event->x());
    }
    update();
}

void LayerListView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(LayerListView);

    if (event->button() != Qt::LeftButton)
        return;

    int clickedRow = qMax(qMin(event->y() / d->rowSize.height(), things.size() - 1), 0);
    int clickedColumn = d->clickedColumn(event->x());

    if (d->nameEditor->isVisible() &&
        (clickedRow != selectedThing || clickedColumn != NameColumn))
    {
        things[selectedThing].name = d->nameEditor->text();
        d->nameEditor->hide();
        emit edited(rowFixup(selectedThing), NameColumn,
                    QVariant::fromValue<QString>(things[selectedThing].name));
        recalulateSize();
    }
    else if (clickedColumn == VisibleColumn)
    {
        things[clickedRow].visible = !things[clickedRow].visible;
        emit edited(rowFixup(clickedRow), VisibleColumn,
                    QVariant::fromValue<bool>(things[clickedRow].visible));
    }
    else if (clickedColumn == EditableColumn)
    {
        things[clickedRow].editable = !things[clickedRow].editable;
        emit edited(rowFixup(clickedRow), EditableColumn,
                    QVariant::fromValue<bool>(things[clickedRow].editable));
    }
    else if (clickedColumn == NameColumn)
    {
        selectedThing = clickedRow;
        emit selectionChanged(rowFixup(selectedThing));
    }

    d->hasFocus = false;
    update();
}

void LayerListView::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(LayerListView);

    if (!event->buttons().testFlag(Qt::LeftButton))
        return;
    if (d->nameEditor->isVisible())
        return;

    int clickedRow = qMax(qMin(event->y() / d->rowSize.height(), things.size() - 1), 0);
    int clickedColumn = d->clickedColumn(event->x());

    d->focusRow = clickedRow;
    d->focusColumn = clickedColumn;
    update();
}

bool LayerListView::eventFilter(QObject *watched, QEvent *event)
{
    Q_D(LayerListView);

    if (watched == d->nameEditor && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Enter ||
            key->key() == Qt::Key_Return)
        {
            things[selectedThing].name = d->nameEditor->text();
            d->nameEditor->hide();
            emit edited(rowFixup(selectedThing), NameColumn,
                        QVariant::fromValue<QString>(things[selectedThing].name));
            recalulateSize();
            return true;
        }
        else if (key->key() == Qt::Key_Escape)
        {
            d->nameEditor->hide();
            return true;
        }
    }

    return false;
}

void LayerListView::recalulateSize()
{
    Q_D(LayerListView);

    int textWidth = 0;

    for (CanvasWidget::LayerInfo const &thing: things)
    {
        textWidth = qMax(textWidth, fontMetrics().boundingRect(thing.name).width());
    }

    d->rowSize.setWidth(d->iconSize * 2 + d->hPad * 4 + textWidth);

    emit updateGeometry();
}

int LayerListView::rowFixup(int row)
{
    return (things.size() - row - 1);
}