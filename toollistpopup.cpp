#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QVariant>
#include <QDebug>

#include "toollistpopup.h"

class ToolListPopupPrivate
{
public:
    QWidget *parentWidget;
    QScrollArea *scrollArea;
    QWidget *scrollContents;
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

    layout = new QVBoxLayout();
    layout->setSpacing(3);
    layout->setContentsMargins(0, 0, 0, 0);
    d->scrollContents = new QWidget(this);
    d->scrollContents->setLayout(layout);

    d->scrollArea->setWidget(d->scrollContents);
    d->scrollArea->setWidgetResizable(true);
}

void ToolListPopup::setToolList(const ToolList &list)
{
    Q_D(ToolListPopup);
    toolList = list;

    QBoxLayout *toolboxLayout = qobject_cast<QBoxLayout *>(d->scrollContents->layout());
    Q_ASSERT(toolboxLayout);

    QList<QWidget *>toolButtons = d->scrollContents->findChildren<QWidget *>();
    for (int i = 0; i < toolButtons.size(); ++i) {
        delete toolButtons[i];
    }

    for (int i = 0; i < toolList.size(); ++i)
    {
        QString toolPath = toolList.at(i).first;
        QString name = toolList.at(i).second;

        QPushButton *button = new QPushButton(name);
        button->setProperty("toolName", QString(toolPath));
        button->setCheckable(true);
        connect(button, &QPushButton::clicked, this, &ToolListPopup::toolButtonClicked);
        toolboxLayout->addWidget(button);
    }


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
    QList<QPushButton *>toolButtons = findChildren<QPushButton *>();
    for (int i = 0; i < toolButtons.size(); ++i)
    {
        QPushButton *button = toolButtons[i];
        if (button->property("toolName").toString() == toolPath)
            button->setChecked(true);
        else
            button->setChecked(false);
    }
}

void ToolListPopup::toolButtonClicked()
{
    QVariant toolNameProp = sender()->property("toolName");
    if (toolNameProp.type() == QVariant::String)
    {
        if (ToolListWidget *toolWidget = qobject_cast<ToolListWidget *>(parent()))
            toolWidget->pickTool(toolNameProp.toString());
    }
    else
        qWarning() << sender()->objectName() << " has no toolName property.";

    hide();
}
