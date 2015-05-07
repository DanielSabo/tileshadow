#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QVariant>
#include <QDebug>

#include "toollistpopup.h"
#include "toollistview.h"

class ToolListPopupPrivate
{
public:
    QWidget *parentWidget;
    QScrollArea *scrollArea;
    ToolListView *scrollContents;
};

ToolListPopup::ToolListPopup(QWidget *parent) :
    QWidget(parent, Qt::Popup),
    d_ptr(new ToolListPopupPrivate)
{
    Q_D(ToolListPopup);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(3);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    d->parentWidget = parent;
    d->scrollArea = new QScrollArea(this);
    layout->addWidget(d->scrollArea);

    d->scrollContents = new ToolListView(this);

    d->scrollArea->setWidget(d->scrollContents);
    d->scrollArea->setWidgetResizable(true);

    connect(d->scrollContents, &ToolListView::selectionChanged, this, &ToolListPopup::toolSelected);
}

void ToolListPopup::setToolList(const ToolList &list)
{
    Q_D(ToolListPopup);
    d->scrollContents->setToolList(list);

    int sbWidth = d->scrollArea->verticalScrollBar()->style()->pixelMetric(QStyle::PM_ScrollBarExtent, 0, d->scrollArea->verticalScrollBar());
    int frameWidth = d->scrollArea->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, d->scrollArea);
    int contentWidth = d->scrollArea->sizeHint().width();
    d->scrollArea->setMinimumWidth(contentWidth + sbWidth + frameWidth * 2);
}

void ToolListPopup::reposition(QRect const &globalBounds, QPoint const &globalOrigin)
{
    Q_D(ToolListPopup);
    show();

    int desiredHeight = d->scrollArea->frameWidth() * 2 + d->scrollContents->sizeHint().height();
    int h = qMin(desiredHeight, globalBounds.height());

    if (h > globalBounds.height())
        h = globalBounds.height();

    //FIXME: setMaximumHeight call shouldn't be needed but avoids Qt 5.2 setting things in the wrong order
    setMaximumHeight(h);
    setFixedHeight(h);

    int left = globalOrigin.x() - width();
    int top = globalOrigin.y() - h / 2;

    if (top + h > globalBounds.y() + globalBounds.height())
        top -= (top + h) - (globalBounds.y() + globalBounds.height());
    else if (top < globalBounds.top())
        top = globalBounds.top();

    move(left, top);
}

void ToolListPopup::setActiveTool(QString const &toolPath)
{
    Q_D(ToolListPopup);
    d->scrollContents->setActiveTool(toolPath);
}

void ToolListPopup::toolSelected(QString const &toolPath)
{
    if (ToolListWidget *toolWidget = qobject_cast<ToolListWidget *>(this->parent()))
        toolWidget->pickTool(toolPath);
    hide();
}
