#include "layerlistview.h"
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QApplication>
#include <QLineEdit>
#include <QDrag>
#include <QMimeData>

namespace {
enum class DragFocus {
    Above,
    Below,
    None
};
}

class LayerListViewPrivate
{
public:
    int vPad;
    int hPad;
    int iconSize;
    int indentSize;

    bool hasFocus;
    int focusRow;
    int focusColumn;
    QPoint clickPoint;

    QFont unnamedItemFont;
    QString unnamedItemText;
    int unnamedItemWidth;

    QSize rowSize;

    QLineEdit *nameEditor;

    QString dragMimeType;
    DragFocus dragFocus;
    int dragSelection;
    int dragIndent;

    struct RowEntry {
        RowEntry(CanvasWidget::LayerInfo const &entry)
            : name(entry.name),
              visible(entry.visible),
              parentVisible(true),
              editable(entry.editable),
              parentEditable(true),
              type(entry.type),
              absoluteIndex(entry.index) {}

        QString name;
        bool visible;
        bool parentVisible;
        bool editable;
        bool parentEditable;
        LayerType::Type type;
        int absoluteIndex;
        int indent;
    };

    QList<RowEntry> things;
    int selectedThing;

    int clickedRow(int y) {
        int row = y / rowSize.height();
        return qBound(0, row, things.size() - 1);
    }

    int clickedColumn(int x) {
        if (x <= (hPad * 2 + iconSize))
            return LayerListView::VisibleColumn;
        else if (x <= hPad * 3 + iconSize * 2)
            return LayerListView::EditableColumn;
        else
            return LayerListView::NameColumn;
    }

    int parentForIndent(int target_indent, int row)
    {
        while (row > 0)
        {
            if (things[row].indent <= target_indent)
                return row;
            row--;
        }
        return row;
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
    setAcceptDrops(true);

    d->dragMimeType = QStringLiteral("application/x-%1-%2-layer-id").arg(QCoreApplication::applicationName().toLower()).arg(QCoreApplication::applicationPid());
    d->dragFocus = DragFocus::None;

    d->unnamedItemFont = font();
    d->unnamedItemFont.setItalic(true);
    d->unnamedItemText = tr("Unnamed");
    d->unnamedItemWidth = QFontMetrics(d->unnamedItemFont).boundingRect(d->unnamedItemText).width();

    visibleIcon = QIcon(":/icons/tileshadow-visible.png");
    visibleIcon.addFile(":/icons/tileshadow-visible.png", QSize(), QIcon::Normal, QIcon::On);
    visibleIcon.addFile(":/icons/tileshadow-visible-off.png", QSize(), QIcon::Normal, QIcon::Off);
    visibleIcon.addFile(":/icons/tileshadow-visible-disabled-off.png", QSize(), QIcon::Disabled, QIcon::Off);
    lockedIcon = QIcon(":/icons/tileshadow-lock.png");
    lockedIcon.addFile(":/icons/tileshadow-lock.png", QSize(), QIcon::Normal, QIcon::On);
    lockedIcon.addFile(":/icons/tileshadow-lock-off.png", QSize(), QIcon::Normal, QIcon::Off);
    lockedIcon.addFile(":/icons/tileshadow-lock-disabled-on.png", QSize(), QIcon::Disabled, QIcon::On);

    d->hPad = 2;
    d->vPad = 1;
    d->iconSize = 16;
    d->indentSize = 16;
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

LayerListView::~LayerListView()
{
}

QSize LayerListView::sizeHint() const
{
    Q_D(const LayerListView);

    return QSize(d->rowSize.width(),
                 d->rowSize.height() * qMax<int>(1, d->things.size()));
}

int LayerListView::rowHeight()
{
    Q_D(const LayerListView);

    return d->rowSize.height();
}

namespace {
    void appendEntries(QList<LayerListViewPrivate::RowEntry> &entryList,
                       const QList<CanvasWidget::LayerInfo> &infoList,
                       int indent, bool parentVisible, bool parentEditable) {
        for (auto const &info: infoList)
        {
            LayerListViewPrivate::RowEntry entry = {info};
            entry.indent = indent;
            entry.parentVisible = parentVisible;
            entry.parentEditable = parentEditable;
            if (!info.children.empty())
                appendEntries(entryList, info.children, indent + 1, entry.visible && parentVisible, entry.editable && parentEditable);
            entryList.prepend(entry);
        }
    };
}

void LayerListView::setData(const QList<CanvasWidget::LayerInfo> &data, int selection)
{
    Q_D(LayerListView);

    d->nameEditor->hide();

    d->things.clear();
    appendEntries(d->things, data, 0, true, true);
    d->selectedThing = 0;
    for (int i = 0; i < d->things.size(); ++i)
    {
        if (d->things.at(i).absoluteIndex == selection)
        {
            d->selectedThing = i;
            break;
        }
    }

    recalulateSize();
    update();
}

int LayerListView::getSelectedRow()
{
    Q_D(LayerListView);
    return d->things[d->selectedThing].absoluteIndex;
}

void LayerListView::paintEvent(QPaintEvent *event)
{
    Q_D(LayerListView);

    QPainter painter(this);

    QSize widgetRowSize(width(), d->rowSize.height());
    int highlightRow = d->selectedThing;
    if (d->dragFocus != DragFocus::None)
        highlightRow = d->dragSelection;

    for (int i = 0; i < d->things.size(); ++i)
    {
        auto const &thing = d->things.at(i);

        QPoint rowOrigin = QPoint(0, d->rowSize.height() * i);
        QPoint contentOrigin = rowOrigin + QPoint(d->hPad, d->vPad);

        if (d->hasFocus && i == d->focusRow && d->focusColumn == NameColumn)
            painter.fillRect(QRect(rowOrigin, widgetRowSize), palette().brush(QPalette::Normal, QPalette::Highlight));
        else if (i == highlightRow)
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

        if (!thing.parentVisible)
            visibleIcon.paint(&painter, visibleIconRect, Qt::AlignCenter, QIcon::Disabled, QIcon::Off);
        else if (thing.visible)
            visibleIcon.paint(&painter, visibleIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
        else
            visibleIcon.paint(&painter, visibleIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        if (!thing.parentEditable)
            lockedIcon.paint(&painter, lockIconRect, Qt::AlignCenter, QIcon::Disabled, QIcon::On);
        else if (!thing.editable)
            lockedIcon.paint(&painter, lockIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
        else
            lockedIcon.paint(&painter, lockIconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);

        painter.save();
        painter.setPen(palette().color(QPalette::Normal, QPalette::Mid));
        int textIndent = thing.indent;
        if (thing.type == LayerType::Group)
        {
            int x0 = lockIconRect.x() + visibleIconRect.width() + d->hPad + d->indentSize * thing.indent;
            int y0 = contentOrigin.y() + (widgetRowSize.height() - d->vPad * 2) / 2;
            int x1 = x0 + d->indentSize / 2;
            int y1 = contentOrigin.y() + (widgetRowSize.height() - d->vPad * 2);

            if (thing.indent > 0)
                x0 -= + d->indentSize / 2;

            painter.drawLine(QLine{x0, y0, x1, y0});
            painter.drawLine(QLine{x1, y0, x1, y1});
            textIndent += 1;
        }

        for (int indent = 1; indent <= thing.indent; ++indent)
        {
            int y0 = rowOrigin.y();
            int x0 = lockIconRect.x() + visibleIconRect.width() + d->hPad + d->indentSize * indent - d->indentSize / 2;
            int y1 = contentOrigin.y() + (widgetRowSize.height() - d->vPad * 2);
            painter.drawLine(QLine{x0, y0, x0, y1});
        }
        painter.restore();

        QPoint textOrigin = QPoint(lockIconRect.x() + visibleIconRect.width() + d->hPad + d->indentSize * textIndent,
                                   contentOrigin.y());
        QRect textRect = QRect(textOrigin,
                               QSize(widgetRowSize.width() - textOrigin.x() - d->hPad,
                                     widgetRowSize.height() - d->vPad * 2));

        painter.save();
        if (d->hasFocus && i == d->focusRow && d->focusColumn == NameColumn)
            painter.setPen(palette().color(QPalette::Normal, QPalette::HighlightedText));
        else if (i == highlightRow)
            painter.setPen(palette().color(QPalette::Inactive, QPalette::HighlightedText));
        else
            painter.setPen(palette().color(QPalette::Normal, QPalette::Text));

        if (thing.name.isEmpty())
        {
            painter.setFont(d->unnamedItemFont);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, d->unnamedItemText);
        }
        else
        {
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, thing.name);
        }

        painter.restore();
    }

    if (d->dragFocus != DragFocus::None)
    {
        int paintRow = d->focusRow;
        int paintIndent = d->dragIndent;

        if (d->dragFocus == DragFocus::Below)
            paintRow += 1;

        int x = (d->iconSize + d->hPad) * 2 + d->indentSize * paintIndent;
        int y = d->rowSize.height() * paintRow - d->vPad * 2;
        int w = widgetRowSize.width();
        int h = d->vPad * 4;

        QRect fillRect{x, y, w, h};
        painter.fillRect(fillRect, palette().brush(QPalette::Normal, QPalette::Highlight));
    }
}

void LayerListView::mousePressEvent(QMouseEvent *event)
{
    Q_D(LayerListView);

    if (event->button() != Qt::LeftButton)
        return;
    if (d->nameEditor->isVisible())
        return;

    int clickedRow = d->clickedRow(event->y());
    int clickedColumn = d->clickedColumn(event->x());

    if (event->type() == QEvent::MouseButtonDblClick &&
        clickedColumn == NameColumn)
    {
        d->selectedThing = clickedRow;
        d->nameEditor->setText(d->things[clickedRow].name);
        d->nameEditor->selectAll();
        int leadingWidth = d->iconSize * 2 + d->hPad * 2;
        d->nameEditor->move(leadingWidth, d->rowSize.height() * d->selectedThing);
        d->nameEditor->setFixedWidth(width() - leadingWidth - d->hPad);
        d->nameEditor->show();
        d->nameEditor->setFocus(Qt::MouseFocusReason);
    }
    else
    {
        d->hasFocus = true;
        d->focusRow = clickedRow;
        d->focusColumn = d->clickedColumn(event->x());
        d->clickPoint = event->pos();
    }
    update();
}

void LayerListView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(LayerListView);

    if (event->button() != Qt::LeftButton)
        return;

    int clickedRow = d->clickedRow(event->y());
    int clickedColumn = d->clickedColumn(event->x());

    if (d->nameEditor->isVisible())
    {
        if (clickedRow != d->selectedThing || clickedColumn != NameColumn)
        {
            d->things[d->selectedThing].name = d->nameEditor->text();
            d->nameEditor->hide();
            emit edited(d->things[d->selectedThing].absoluteIndex, NameColumn,
                        QVariant::fromValue<QString>(d->things[d->selectedThing].name));
            recalulateSize();
        }
    }
    else if (d->hasFocus)
    {
        if (clickedColumn == VisibleColumn)
        {
            d->things[clickedRow].visible = !d->things[clickedRow].visible;
            emit edited(d->things[clickedRow].absoluteIndex, VisibleColumn,
                        QVariant::fromValue<bool>(d->things[clickedRow].visible));
        }
        else if (clickedColumn == EditableColumn)
        {
            d->things[clickedRow].editable = !d->things[clickedRow].editable;
            emit edited(d->things[clickedRow].absoluteIndex, EditableColumn,
                        QVariant::fromValue<bool>(d->things[clickedRow].editable));
        }
        else if (clickedColumn == NameColumn)
        {
            d->selectedThing = clickedRow;
            emit selectionChanged(d->things[d->selectedThing].absoluteIndex);
        }
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

    if ((d->clickPoint - event->pos()).manhattanLength() > d->rowSize.height() / 2 &&
         d->focusColumn == LayerListView::NameColumn)
    {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData();

        d->dragSelection = d->focusRow;
        QByteArray layerdata;
        QDataStream(&layerdata, QIODevice::WriteOnly) << d->things[d->dragSelection].absoluteIndex;
        mimeData->setData(d->dragMimeType, layerdata);
        drag->setMimeData(mimeData);
        drag->setPixmap(QPixmap(":icons/image-x-generic.png"));

        d->hasFocus = false;
        d->dragFocus = DragFocus::None;

        Qt::DropAction drop = drag->exec(Qt::MoveAction);
        DragFocus dropFocus = d->dragFocus;
        d->dragFocus = DragFocus::None;

        if (drop == Qt::MoveAction && dropFocus != DragFocus::None)
        {
            int source = d->things[d->dragSelection].absoluteIndex;
            int target = d->things[d->focusRow].absoluteIndex;

            if (dropFocus == DragFocus::Above)
            {
                shuffleLayers(source, target, LayerShuffle::Above);
            }
            else if (dropFocus == DragFocus::Below)
            {
                if ((d->things[d->focusRow].type == LayerType::Group) &&
                    (d->dragIndent > d->things[d->focusRow].indent))
                {
                    shuffleLayers(source, target, LayerShuffle::Into);
                }
                else
                {
                    int parentRow = d->parentForIndent(d->dragIndent, d->focusRow);
                    target = d->things[parentRow].absoluteIndex;
                    shuffleLayers(source, target, LayerShuffle::Below);
                }
            }
        }

        update();

        // The drag consumes the mouse release event
    }
    else
    {
        int clickedRow = d->clickedRow(event->y());
        int clickedColumn = d->clickedColumn(event->x());

        d->hasFocus = (d->focusRow == clickedRow && d->focusColumn == clickedColumn && rect().contains(event->pos()));
        update();
    }
}

void LayerListView::dragEnterEvent(QDragEnterEvent *event)
{
    Q_D(LayerListView);

    if (event->mimeData()->hasFormat(d->dragMimeType))
    {
        event->acceptProposedAction();
    }
}

void LayerListView::dragMoveEvent(QDragMoveEvent *event)
{
    Q_D(LayerListView);

    QPoint pos = event->pos();

    int clickedRow = d->clickedRow(pos.y());
    int clickedColumn = d->clickedColumn(pos.x());
    d->focusRow = clickedRow;
    d->focusColumn = clickedColumn;
    if (pos.y() - clickedRow * d->rowSize.height() < d->rowSize.height() / 2)
        d->dragFocus = DragFocus::Above;
    else
        d->dragFocus = DragFocus::Below;

    int maxIndent = d->things[d->focusRow].indent;
    int minIndent = 0;
    if (d->dragFocus == DragFocus::Below)
    {
        if (d->things[d->focusRow].type == LayerType::Group)
            maxIndent += 1;

        if (d->focusRow < d->things.size() - 1)
            minIndent = qMin(d->things[d->focusRow + 1].indent, maxIndent);
        else
            minIndent = 0;
    }
    else
    {
        minIndent = maxIndent;
    }

    d->dragIndent = qBound(minIndent, (pos.x() - (d->iconSize + d->hPad) * 2) / d->indentSize, maxIndent);

    QWidget::dragMoveEvent(event);
    update();
}

void LayerListView::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_D(LayerListView);
    d->dragFocus = DragFocus::None;

    QWidget::dragLeaveEvent(event);
    update();
}

void LayerListView::dropEvent(QDropEvent *event)
{
    Q_D(LayerListView);

    if (event->mimeData()->hasFormat(d->dragMimeType))
    {
        event->acceptProposedAction();
    }
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
            d->things[d->selectedThing].name = d->nameEditor->text();
            d->nameEditor->hide();
            emit edited(d->things[d->selectedThing].absoluteIndex, NameColumn,
                        QVariant::fromValue<QString>(d->things[d->selectedThing].name));
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

    int textWidth = d->unnamedItemWidth;

    for (auto const &thing: d->things)
    {
        textWidth = qMax(textWidth, d->indentSize * thing.indent + fontMetrics().boundingRect(thing.name).width());
    }

    d->rowSize.setWidth(d->iconSize * 2 + d->hPad * 4 + textWidth);

    emit updateGeometry();
}
