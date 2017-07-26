#include "toollistwidget.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QDebug>
#include "canvaswidget.h"
#include "toolfactory.h"
#include "sidebarpopup.h"
#include "toollistview.h"

class ToolListWidgetPrivate
{
public:
    QPushButton *toolPopupButton = nullptr;
    SidebarPopup *popup = nullptr;
    ToolListView *toolListView = nullptr;
};

ToolListWidget::ToolListWidget(CanvasWidget *canvas, QWidget *parent) :
    QWidget(parent),
    canvas(canvas),
    d_ptr(new ToolListWidgetPrivate)
{
    Q_D(ToolListWidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(3);
    layout->setContentsMargins(0, 0, 0, 0);

    d->popup = new SidebarPopup(this);
    QVBoxLayout *popup_layout = new QVBoxLayout(d->popup);
    popup_layout->setSpacing(3);
    popup_layout->setContentsMargins(1, 1, 1, 1);
    d->toolListView = new ToolListView();
    popup_layout->addWidget(d->toolListView);
    connect(d->toolListView, &ToolListView::selectionChanged, this, [this, d](QString const &toolPath) {
        d->popup->hide();
        pickTool(toolPath);
    });

    d->toolPopupButton = new QPushButton("Tools...");
    connect(d->toolPopupButton, &QPushButton::clicked, this, &ToolListWidget::showPopup);
    layout->addWidget(d->toolPopupButton);

    connect(canvas, &CanvasWidget::updateTool, this, &ToolListWidget::updateTool);
    reloadTools();
    updateTool();
}

ToolListWidget::~ToolListWidget()
{
}

void ToolListWidget::reloadTools()
{
    Q_D(ToolListWidget);

    d->toolListView->setToolList(ToolFactory::listTools());
}


void ToolListWidget::updateTool()
{
    Q_D(ToolListWidget);

    QString activeTool = canvas->getActiveTool();
    d->toolListView->setActiveTool(activeTool);
}

void ToolListWidget::pickTool(QString const &toolPath)
{
    canvas->setActiveTool(toolPath);
}

void ToolListWidget::showPopup()
{
    Q_D(ToolListWidget);

    d->popup->reposition(this, d->toolPopupButton);
}
