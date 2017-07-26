#include "sidebarpopup.h"
#include <QLayout>
#include <QDebug>


SidebarPopup::SidebarPopup(QWidget *parent) :
    QWidget(parent, Qt::Popup)
{
}

SidebarPopup::~SidebarPopup()
{
}

void SidebarPopup::reposition(QWidget *sidebar, QWidget *source)
{
    int originY = source->mapToGlobal(source->pos()).y() + source->height() / 2;
    int originX = sidebar->mapToGlobal(sidebar->pos()).x();

    reposition(sidebar->window()->frameGeometry(), QPoint(originX, originY));
}

void SidebarPopup::reposition(QRect const &globalBounds, QPoint const &globalOrigin)
{
    show();

    int h = globalBounds.height();

    if (QLayout *contentLayout = layout())
        h = qMin(h, contentLayout->sizeHint().height());

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
