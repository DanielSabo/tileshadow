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
    ToolList toolList;
    QPushButton *toolPopupButton;
};

ToolListWidget::ToolListWidget(QWidget *parent) :
    QWidget(parent),
    canvas(NULL),
    popup(NULL),
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
}

ToolListWidget::~ToolListWidget()
{
    if (canvas)
        disconnect(canvas, 0, this, 0);
    canvas = NULL;
}

void ToolListWidget::reloadTools()
{
    Q_D(ToolListWidget);

    d->toolList = ToolFactory::listTools();
}

void ToolListWidget::setCanvas(CanvasWidget *newCanvas)
{
    if (canvas)
        disconnect(canvas, 0, this, 0);

    canvas = newCanvas;

    if (canvas)
    {
        connect(canvas, SIGNAL(updateTool()), this, SLOT(updateTool()));
        connect(canvas, SIGNAL(destroyed(QObject *)), this, SLOT(canvasDestroyed(QObject *)));
    }

    updateTool();
}

void ToolListWidget::canvasDestroyed(QObject *obj)
{
    canvas = NULL;
}

void ToolListWidget::updateTool()
{
    if (!canvas)
        return;

    QString activeTool = canvas->getActiveTool();

    if (popup)
        popup->setActiveTool(activeTool);
}

void ToolListWidget::pickTool(QString const &toolPath)
{
    if (canvas)
        canvas->setActiveTool(toolPath);
}

void ToolListWidget::showPopup()
{
    Q_D(ToolListWidget);

    if (!popup)
        popup = new ToolListPopup(this);
    popup->setToolList(d->toolList);
    if (canvas)
        popup->setActiveTool(canvas->getActiveTool());

    QPoint buttonPos = d->toolPopupButton->mapToGlobal(d->toolPopupButton->pos());
    int originY = buttonPos.y() + d->toolPopupButton->height() / 2;
    int originX = mapToGlobal(pos()).x();

    popup->reposition(window()->frameGeometry(), QPoint(originX, originY));
}
