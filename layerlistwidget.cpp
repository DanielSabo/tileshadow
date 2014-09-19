#include "layerlistwidget.h"
#include "ui_layerlistwidget.h"

#include <QComboBox>
#include <QDebug>

#include "canvaswidget.h"

LayerListWidget::LayerListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LayerListWidget),
    canvas(NULL),
    freezeLayerList(false)
{
    ui->setupUi(this);

    ui->layerModeComboBox->addItem(tr("Normal"), QVariant(BlendMode::Over));
    ui->layerModeComboBox->addItem(tr("Multiply"), QVariant(BlendMode::Multiply));
    ui->layerModeComboBox->addItem(tr("Dodge"), QVariant(BlendMode::ColorDodge));
    ui->layerModeComboBox->addItem(tr("Burn"), QVariant(BlendMode::ColorBurn));
    ui->layerModeComboBox->addItem(tr("Screen"), QVariant(BlendMode::Screen));

    connect(ui->addLayerButton,  SIGNAL(clicked()), this, SLOT(layerListAdd()));
    connect(ui->delLayerButton,  SIGNAL(clicked()), this, SLOT(layerListRemove()));
    connect(ui->dupLayerButton,  SIGNAL(clicked()), this, SLOT(layerListDuplicate()));
    connect(ui->downLayerButton, SIGNAL(clicked()), this, SLOT(layerListMoveDown()));
    connect(ui->upLayerButton,   SIGNAL(clicked()), this, SLOT(layerListMoveUp()));

    connect(ui->layerList, &LayerListView::edited, [this](int row, int column, QVariant data){
        if (!canvas)
            return;

        if (freezeLayerList)
            return;

        freezeLayerList = true;
        if (column == LayerListView::VisibleColumn)
            canvas->setLayerVisible(row, data.toBool());
        else if (column == LayerListView::EditableColumn)
            canvas->setLayerEditable(row, data.toBool());
        else if (column == LayerListView::NameColumn)
            canvas->renameLayer(row, data.toString());
        freezeLayerList = false;
    });

    connect(ui->layerList, &LayerListView::selectionChanged, [this](int row){
       if (!canvas)
           return;

       if (freezeLayerList)
           return;

       freezeLayerList = true;
       canvas->setActiveLayer(row);
       BlendMode::Mode currentMode = canvas->getLayerMode(row);
       ui->layerModeComboBox->setCurrentIndex(ui->layerModeComboBox->findData(QVariant(currentMode)));
       freezeLayerList = false;
    });

    connect(ui->layerModeComboBox, SIGNAL(activated(int)), this, SLOT(layerModeActivated(int)));
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
    QList<CanvasWidget::LayerInfo> layerList = canvas->getLayerList();
    int currentLayer = canvas->getActiveLayer();
    ui->layerList->setData(layerList, currentLayer);

    BlendMode::Mode currentMode;

    if (layerList.size())
        currentMode = layerList.at(currentLayer).mode;
    else
        currentMode = BlendMode::Over;

    ui->layerModeComboBox->setCurrentIndex(ui->layerModeComboBox->findData(QVariant(currentMode)));
    freezeLayerList = false;
}

void LayerListWidget::layerModeActivated(int index)
{
    /* Don't update the selection while the list is changing */
    if (freezeLayerList)
        return;

    freezeLayerList = true;
    BlendMode::Mode newMode = BlendMode::Mode(ui->layerModeComboBox->itemData(index).toInt());
    ui->layerModeComboBox->setCurrentIndex(index);

    canvas->setLayerMode(ui->layerList->getSelectedRow(), newMode);
    freezeLayerList = false;
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

void LayerListWidget::layerListDuplicate()
{
    if (!canvas)
        return;

    canvas->duplicateLayer(canvas->getActiveLayer());
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
