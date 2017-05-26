#include "toollistwidget.h"

#include <QVBoxLayout>
#include <QPushButton>

#include <QDebug>

#include "canvaswidget.h"
#include "toollistpopup.h"
#include "toolfactory.h"

class ToolListWidgetPrivate
{
public:
    QPushButton *toolPopupButton;
};

ToolListWidget::ToolListWidget(CanvasWidget *canvas, QWidget *parent) :
    QWidget(parent),
    canvas(canvas),
    popup(new ToolListPopup(this)),
    d_ptr(new ToolListWidgetPrivate)
{
    Q_D(ToolListWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(3);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    d->toolPopupButton = new QPushButton("Tools...");
    connect(d->toolPopupButton, SIGNAL(clicked()), this, SLOT(showPopup()));
    layout->addWidget(d->toolPopupButton);

    connect(canvas, &CanvasWidget::updateTool, this, &ToolListWidget::updateTool);
    connect(popup, &ToolListPopup::toolSelected, this, &ToolListWidget::pickTool);
    reloadTools();
    updateTool();
}

ToolListWidget::~ToolListWidget()
{
}

void ToolListWidget::reloadTools()
{
    popup->setToolList(ToolFactory::listTools());
}


void ToolListWidget::updateTool()
{
    QString activeTool = canvas->getActiveTool();
    popup->setActiveTool(activeTool);
}

void ToolListWidget::pickTool(QString const &toolPath)
{
    canvas->setActiveTool(toolPath);
}

void ToolListWidget::showPopup()
{
    Q_D(ToolListWidget);

    QPoint buttonPos = d->toolPopupButton->mapToGlobal(d->toolPopupButton->pos());
    int originY = buttonPos.y() + d->toolPopupButton->height() / 2;
    int originX = mapToGlobal(pos()).x();

    popup->reposition(window()->frameGeometry(), QPoint(originX, originY));
}
