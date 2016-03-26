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
    ToolListView *toolListView;
};

ToolListPopup::ToolListPopup(QWidget *parent) :
    QWidget(parent, Qt::Popup),
    d_ptr(new ToolListPopupPrivate)
{
    Q_D(ToolListPopup);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(3);
    layout->setContentsMargins(1, 1, 1, 1);
    setLayout(layout);

    d->toolListView = new ToolListView(this);
    layout->addWidget(d->toolListView);

    connect(d->toolListView, &ToolListView::selectionChanged, this, [this](QString const &toolPath) {
        hide();
        toolSelected(toolPath);
    });
}

void ToolListPopup::setToolList(const ToolList &list)
{
    Q_D(ToolListPopup);
    d->toolListView->setToolList(list);
}

void ToolListPopup::reposition(QRect const &globalBounds, QPoint const &globalOrigin)
{
    Q_D(ToolListPopup);
    show();

    int desiredHeight = d->toolListView->sizeHint().height();
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
    d->toolListView->setActiveTool(toolPath);
}
