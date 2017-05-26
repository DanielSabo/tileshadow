#include "toollistview.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>
#include <QScrollBar>

class ToolListViewPrivate
{
public:
    int hPad;
    int vPad;
    QSize rowSize;

    QString selectedToolPath;
    ToolList tools;

    QPoint mousePressLocation;
    bool mousePress;
    bool dragScroll;
    int dragScrollInitialValue;
};

ToolListView::ToolListView(QWidget *parent)
    : QAbstractScrollArea(parent),
      d_ptr(new ToolListViewPrivate)
{
    Q_D(ToolListView);

    setPalette(QApplication::palette("QAbstractItemView"));
    QFont itemFont = QApplication::font("QAbstractItemView");
    if (itemFont.pointSizeF() > 0)
        itemFont.setPointSizeF(itemFont.pointSizeF() + 2);
    setFont(itemFont);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    d->hPad = 4;
    d->vPad = 2;
    d->rowSize = QSize(100, fontMetrics().height() + d->vPad * 2);

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

ToolListView::~ToolListView()
{
}

QSize ToolListView::viewportSizeHint() const
{
    Q_D(const ToolListView);

    return QSize(d->rowSize.width(),
                 d->rowSize.height() * qMax<int>(1, d->tools.size()));
}

void ToolListView::paintEvent(QPaintEvent *event)
{
    Q_D(ToolListView);

    QPainter painter(viewport());

    QSize widgetRowSize(width(), d->rowSize.height());
    painter.setPen(palette().color(QPalette::Normal, QPalette::Text));

    const int scrollOffset = verticalScrollBar()->value();

    for (int i = 0; i < d->tools.size(); ++i)
    {
        QString const &toolPath = d->tools.at(i).first;
        QString const &toolName = d->tools.at(i).second;

        QPoint rowOrigin = QPoint(0, d->rowSize.height() * i - scrollOffset);
        QRect contentRect = QRect(rowOrigin, widgetRowSize);
        QRect textRect = QRect(rowOrigin, widgetRowSize) - QMargins(d->hPad, d->vPad, d->hPad, d->vPad);

        painter.save();
        painter.setPen(palette().color(QPalette::Normal, QPalette::AlternateBase));
        painter.drawLine(contentRect.topLeft(), contentRect.topRight());
        painter.drawLine(contentRect.bottomLeft(), contentRect.bottomRight());
        painter.restore();

        if (toolPath == d->selectedToolPath)
        {
            painter.save();
            painter.fillRect(contentRect, palette().brush(QPalette::Normal, QPalette::Highlight));
            painter.setPen(palette().color(QPalette::Normal, QPalette::HighlightedText));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, toolName);
            painter.restore();
        }
        else
        {
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, toolName);
        }
    }
}

void ToolListView::mousePressEvent(QMouseEvent *event)
{
    Q_D(ToolListView);

    if (event->button() != Qt::LeftButton)
        return;

    d->mousePressLocation = event->pos();
    d->dragScrollInitialValue = verticalScrollBar()->value();
    d->mousePress = true;
    d->dragScroll = false;
}

void ToolListView::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(ToolListView);

    if (d->mousePress && !d->dragScroll)
    {
        if ((event->pos() - d->mousePressLocation).manhattanLength() > d->rowSize.height())
            d->dragScroll = true;
    }

    if (d->dragScroll)
    {
        verticalScrollBar()->setValue(d->dragScrollInitialValue - event->pos().y() + d->mousePressLocation.y());
    }
}

void ToolListView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(ToolListView);

    if (event->button() != Qt::LeftButton)
        return;

    if (!d->dragScroll)
    {
        int clickedRow = (event->y() + verticalScrollBar()->value()) / d->rowSize.height();
        if (clickedRow >= 0 && clickedRow < d->tools.size())
            selectionChanged(d->tools.at(clickedRow).first);
    }

    d->mousePress = false;
    d->dragScroll = false;
}

void ToolListView::resizeEvent(QResizeEvent *event)
{
    Q_D(ToolListView);
    verticalScrollBar()->setSingleStep(d->rowSize.height());
    verticalScrollBar()->setPageStep(height());
    verticalScrollBar()->setSizeIncrement(1, 1);
    verticalScrollBar()->setRange(0, qMax(0, viewportSizeHint().height() - viewport()->height()));
}

void ToolListView::setToolList(const ToolList &list)
{
    Q_D(ToolListView);
    d->tools = list;

    QFontMetrics metrics = fontMetrics();
    int rowWidth = 100;

    for (auto const &toolEntry: d->tools)
        rowWidth = qMax<int>(rowWidth, metrics.boundingRect(toolEntry.second).width());

    d->rowSize.rwidth() = rowWidth + d->hPad * 2;
    updateGeometry();
}

void ToolListView::setActiveTool(const QString &toolPath)
{
    Q_D(ToolListView);
    d->selectedToolPath = toolPath;
    update();
}
