#include "toollistview.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>

class ToolListViewPrivate
{
public:
    int hPad;
    int vPad;
    QSize rowSize;

    QString selectedToolPath;
    ToolList tools;
};

ToolListView::ToolListView(QWidget *parent)
    : QWidget(parent),
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

    d->hPad = 2;
    d->vPad = 2;
    d->rowSize = QSize(100, fontMetrics().height() + d->vPad * 2);

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QSize ToolListView::sizeHint() const
{
    Q_D(const ToolListView);

    return QSize(d->rowSize.width(),
                 d->rowSize.height() * qMax<int>(1, d->tools.size()));
}


void ToolListView::paintEvent(QPaintEvent *event)
{
    Q_D(ToolListView);

    QPainter painter(this);

    QSize widgetRowSize(width(), d->rowSize.height());
    painter.setPen(palette().color(QPalette::Normal, QPalette::Text));

    for (int i = 0; i < d->tools.size(); ++i)
    {
        QString const &toolPath = d->tools.at(i).first;
        QString const &toolName = d->tools.at(i).second;

        QPoint rowOrigin = QPoint(0, d->rowSize.height() * i);
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

}

void ToolListView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(ToolListView);

    if (event->button() != Qt::LeftButton)
        return;

    int clickedRow = event->y() / d->rowSize.height();
    if (clickedRow >= 0 && clickedRow < d->tools.size())
        selectionChanged(d->tools.at(clickedRow).first);
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
