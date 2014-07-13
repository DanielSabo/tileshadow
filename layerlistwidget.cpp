#include "layerlistwidget.h"
#include "ui_layerlistwidget.h"

#include <QListWidget>

#include "canvaswidget.h"

LayerListWidget::LayerListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LayerListWidget),
    canvas(NULL),
    freezeLayerList(false)
{
    ui->setupUi(this);

    connect(ui->addLayerButton,  SIGNAL(clicked()), this, SLOT(layerListAdd()));
    connect(ui->delLayerButton,  SIGNAL(clicked()), this, SLOT(layerListRemove()));
    connect(ui->downLayerButton, SIGNAL(clicked()), this, SLOT(layerListMoveDown()));
    connect(ui->upLayerButton,   SIGNAL(clicked()), this, SLOT(layerListMoveUp()));

    connect(ui->layerList, SIGNAL(currentRowChanged(int)), this, SLOT(layerListSelection(int)));
    connect(ui->layerList, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(layerListItemEdited(QListWidgetItem *)));
}

LayerListWidget::~LayerListWidget()
{
    delete ui;
}

void LayerListWidget::setCanvas(CanvasWidget *newCanvas)
{
    if (canvas)
        disconnect(canvas, 0, this, 0);

    canvas = newCanvas;

    if (canvas)
    {
        connect(canvas, SIGNAL(updateLayers()), this, SLOT(updateLayers()));
        connect(canvas, SIGNAL(destroyed(QObject *)), this, SLOT(canvasDestroyed(QObject *)));
    }

    updateLayers();
}

void LayerListWidget::canvasDestroyed(QObject *obj)
{
    canvas = NULL;
}

void LayerListWidget::updateLayers()
{
    if (!canvas)
        return;

    if (freezeLayerList)
        return;

    freezeLayerList = true;

    QList<CanvasWidget::LayerInfo> canvasLayers = canvas->getLayerList();

    ui->layerList->clear();
    foreach(CanvasWidget::LayerInfo info, canvasLayers)
    {
        QListWidgetItem *item = new QListWidgetItem(info.name);
        item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
        item->setCheckState(info.visible ? Qt::Checked : Qt::Unchecked);
        ui->layerList->addItem(item);
    }
    ui->layerList->setCurrentRow((ui->layerList->count() - 1) - canvas->getActiveLayer());

    freezeLayerList = false;
}

#include <QDebug>

void LayerListWidget::layerListItemEdited(QListWidgetItem * item)
{
    if (!canvas)
        return;

    freezeLayerList = true;
    int layerIdx = (ui->layerList->count() - 1) - ui->layerList->row(item);
    canvas->renameLayer(layerIdx, item->text());
    canvas->setLayerVisible(layerIdx, item->checkState() == Qt::Checked);
    freezeLayerList = false;
}

void LayerListWidget::layerListSelection(int row)
{
    if (!canvas)
        return;

    /* Don't update the selection while the list is changing */
    if (freezeLayerList)
        return;

    int layerIdx = (ui->layerList->count() - 1) - row;
    canvas->setActiveLayer(layerIdx);
}

void LayerListWidget::layerListAdd()
{
    if (!canvas)
        return;

    canvas->addLayerAbove(canvas->getActiveLayer());
}

void LayerListWidget::layerListRemove()
{
    if (!canvas)
        return;

    canvas->removeLayer(canvas->getActiveLayer());
}

void LayerListWidget::layerListMoveUp()
{
    if (!canvas)
        return;

    int activeLayerIdx = canvas->getActiveLayer();
    canvas->moveLayer(activeLayerIdx, activeLayerIdx + 1);
}

void LayerListWidget::layerListMoveDown()
{
    if (!canvas)
        return;

    int activeLayerIdx = canvas->getActiveLayer();
    canvas->moveLayer(activeLayerIdx, activeLayerIdx - 1);
}
