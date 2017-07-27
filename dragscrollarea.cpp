#include "dragscrollarea.h"
#include <QMouseEvent>
#include <QScrollBar>

class DragScrollAreaPrivate
{
public:
    QPoint mousePressLocation;
    bool mousePress = false;
    bool dragScroll = false;
    int dragScrollInitialValue = 0;
};

DragScrollArea::DragScrollArea(QWidget *parent)
    : QAbstractScrollArea(parent),
      d_ptr(new DragScrollAreaPrivate)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

DragScrollArea::~DragScrollArea()
{
}

QSize DragScrollArea::viewportSizeHint() const
{
    QSize row = rowSize();

    return QSize(row.width(), row.height() * qMax<int>(1, rowCount()));
}

void DragScrollArea::mousePressEvent(QMouseEvent *event)
{
    Q_D(DragScrollArea);

    if (event->button() != Qt::LeftButton)
        return;

    d->mousePressLocation = event->pos();
    d->dragScrollInitialValue = verticalScrollBar()->value();
    d->mousePress = true;
    d->dragScroll = false;
}

void DragScrollArea::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(DragScrollArea);

    if (d->mousePress && !d->dragScroll)
    {
        if ((event->pos() - d->mousePressLocation).manhattanLength() > rowSize().height())
            d->dragScroll = true;
    }

    if (d->dragScroll)
    {
        verticalScrollBar()->setValue(d->dragScrollInitialValue - event->pos().y() + d->mousePressLocation.y());
    }
}

void DragScrollArea::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(DragScrollArea);

    if (!d->dragScroll)
        viewportClicked(event);
    else if (event->button() != Qt::LeftButton)
        return;

    d->mousePress = false;
    d->dragScroll = false;
}

void DragScrollArea::viewportClicked(QMouseEvent *)
{
}

void DragScrollArea::resizeEvent(QResizeEvent *event)
{
    verticalScrollBar()->setSingleStep(rowSize().height());
    verticalScrollBar()->setPageStep(height());
    verticalScrollBar()->setSizeIncrement(1, 1);
    verticalScrollBar()->setRange(0, qMax(0, viewportSizeHint().height() - viewport()->height()));
}
