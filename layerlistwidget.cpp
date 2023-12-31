#include "layerlistwidget.h"
#include "ui_layerlistwidget.h"

#include <QComboBox>
#include <QDebug>

#include "canvaswidget.h"

LayerListWidget::LayerListWidget(CanvasWidget *canvas, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LayerListWidget),
    canvas(canvas),
    freezeLayerList(false)
{
    ui->setupUi(this);

    ui->scrollArea->setMinimumHeight(ui->layerList->rowHeight() * 4);

    ui->layerModeComboBox->addItem(tr("Normal"), QVariant(BlendMode::Over));
    ui->layerModeComboBox->addItem(tr("Multiply"), QVariant(BlendMode::Multiply));
    ui->layerModeComboBox->addItem(tr("Dodge"), QVariant(BlendMode::ColorDodge));
    ui->layerModeComboBox->addItem(tr("Burn"), QVariant(BlendMode::ColorBurn));
    ui->layerModeComboBox->addItem(tr("Screen"), QVariant(BlendMode::Screen));
    ui->layerModeComboBox->addItem(tr("Hue"), QVariant(BlendMode::Hue));
    ui->layerModeComboBox->addItem(tr("Saturation"), QVariant(BlendMode::Saturation));
    ui->layerModeComboBox->addItem(tr("Color"), QVariant(BlendMode::Color));
    ui->layerModeComboBox->addItem(tr("Luminosity"), QVariant(BlendMode::Luminosity));
    ui->layerModeComboBox->addItem(tr("Erase"), QVariant(BlendMode::DestinationOut));
    ui->layerModeComboBox->addItem(tr("Mask"), QVariant(BlendMode::DestinationIn));
    ui->layerModeComboBox->addItem(tr("Source Atop"), QVariant(BlendMode::SourceAtop));
    ui->layerModeComboBox->addItem(tr("Destination Atop"), QVariant(BlendMode::DestinationAtop));

    connect(ui->addLayerButton,  SIGNAL(clicked()), this, SLOT(layerListAdd()));
    connect(ui->delLayerButton,  SIGNAL(clicked()), this, SLOT(layerListRemove()));
    connect(ui->dupLayerButton,  SIGNAL(clicked()), this, SLOT(layerListDuplicate()));
    connect(ui->downLayerButton, SIGNAL(clicked()), this, SLOT(layerListMoveDown()));
    connect(ui->upLayerButton,   SIGNAL(clicked()), this, SLOT(layerListMoveUp()));

    connect(ui->layerList, &LayerListView::shuffleLayers, this, &LayerListWidget::layerListShuffle);
    connect(ui->layerList, &LayerListView::edited, this, &LayerListWidget::layerListItemEdited);
    connect(ui->layerList, &LayerListView::selectionChanged, this, &LayerListWidget::layerListSelectionChanged);

    connect(ui->layerModeComboBox, SIGNAL(activated(int)), this, SLOT(layerModeActivated(int)));

    connect(ui->opacitySlider, &QSlider::valueChanged, this, &LayerListWidget::opacitySliderMoved);
    connect(ui->opacitySlider, &QSlider::sliderReleased, this, &LayerListWidget::opacitySliderReleased);

    connect(canvas, &CanvasWidget::updateLayers, this, &LayerListWidget::updateLayers);

    updateLayers();
}

LayerListWidget::~LayerListWidget()
{
}

void LayerListWidget::updateLayers()
{
    if (freezeLayerList)
        return;

    freezeLayerList = true;
    QList<CanvasWidget::LayerInfo> layerList = canvas->getLayerList();
    if (layerList.empty())
    {
        setEnabled(false);
    }
    else
    {
        setEnabled(true);
        int currentLayer = canvas->getActiveLayer();
        ui->layerList->setData(layerList, currentLayer);

        BlendMode::Mode currentMode = canvas->getLayerMode(currentLayer);
        float opacity = canvas->getLayerOpacity(currentLayer);

        ui->layerModeComboBox->setCurrentIndex(ui->layerModeComboBox->findData(QVariant(currentMode)));
        ui->opacitySlider->setValue(100.0f * opacity);
    }
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

    updateLayers();
}

void LayerListWidget::opacitySliderMoved(int value)
{
    if (freezeLayerList)
        return;

    freezeLayerList = true;
    float opacity = qBound(0.0f, value / 100.0f, 1.0f);
    canvas->setLayerTransientOpacity(canvas->getActiveLayer(), opacity);
    freezeLayerList = false;
}

void LayerListWidget::opacitySliderReleased()
{
    if (freezeLayerList)
        return;

    freezeLayerList = true;
    float opacity = qBound(0.0f, ui->opacitySlider->value() / 100.0f, 1.0f);
    canvas->setLayerOpacity(canvas->getActiveLayer(), opacity);
    freezeLayerList = false;

    updateLayers();
}

void LayerListWidget::layerListShuffle(int srcIndex, int targetIndex, LayerShuffle::Type op)
{
    canvas->shuffleLayer(srcIndex, targetIndex, op);
}

void LayerListWidget::layerListItemEdited(int row, int column, QVariant const &data)
{
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

    updateLayers();
}

void LayerListWidget::layerListSelectionChanged(int row)
{
    if (freezeLayerList)
        return;

    freezeLayerList = true;
    canvas->setActiveLayer(row);
    canvas->flashCurrentLayer();
    BlendMode::Mode currentMode = canvas->getLayerMode(row);
    ui->layerModeComboBox->setCurrentIndex(ui->layerModeComboBox->findData(QVariant(currentMode)));
    float opacity = canvas->getLayerOpacity(row);
    ui->opacitySlider->setValue(opacity * 100.0f);
    freezeLayerList = false;

    updateLayers();
}

void LayerListWidget::layerListAdd()
{
    canvas->addLayerAbove(canvas->getActiveLayer());
}

void LayerListWidget::layerListRemove()
{
    canvas->removeLayer(canvas->getActiveLayer());
}

void LayerListWidget::layerListDuplicate()
{
    canvas->duplicateLayer(canvas->getActiveLayer());
}

void LayerListWidget::layerListMoveUp()
{
    canvas->moveLayerUp(canvas->getActiveLayer());
}

void LayerListWidget::layerListMoveDown()
{
    canvas->moveLayerDown(canvas->getActiveLayer());
}
