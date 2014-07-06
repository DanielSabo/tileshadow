#include "toolsettingswidget.h"
#include "toolsettingswidget_p.h"

#include <QVBoxLayout>
#include <QLabel>

#include <QDebug>

#include "canvaswidget.h"

ToolSettingsWidgetPrivate::ToolSettingsWidgetPrivate()
    : QObject(),
      canvas(NULL),
      freezeUpdates(false)
{
}

void ToolSettingsWidgetPrivate::colorDialChanged(const QColor &color)
{
    if (freezeUpdates)
        return;

    freezeUpdates = true;
    canvas->setToolColor(color);
    freezeUpdates = false;
}

void ToolSettingsWidgetPrivate::sizeSliderMoved(int value)
{
    if (freezeUpdates)
        return;

    float sizeMultiplyer;

    if (value > 0)
    {
        sizeMultiplyer = value / 10.0f + 1.0f;
    }
    else if (value < 0)
    {
        sizeMultiplyer = 1.0f / (-value / 10.0f + 1.0f);
    }
    else
    {
        sizeMultiplyer = 1.0f;
    }

    freezeUpdates = true;
    canvas->setToolSizeFactor(sizeMultiplyer);
    freezeUpdates = false;
}

ToolSettingsWidget::ToolSettingsWidget(QWidget *parent) :
    QWidget(parent),
    d_ptr(new ToolSettingsWidgetPrivate)
{
    Q_D(ToolSettingsWidget);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(3);
//    layout->setContentsMargins(3, 0, 3, 0);
    layout->setContentsMargins(12, 0, 12, 0);
    setLayout(layout);

    d->toolColorDial = new HSVColorDial(this);
    d->toolColorDial->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    d->toolColorDial->setMinimumSize(QSize(80, 80));
    d->toolColorDial->setBaseSize(QSize(80, 80));
    connect(d->toolColorDial, &HSVColorDial::updateColor, d, &ToolSettingsWidgetPrivate::colorDialChanged);
    layout->addWidget(d->toolColorDial);

    d->toolSizeSlider = new QSlider(Qt::Horizontal, this);
    d->toolSizeSlider->setMinimum(-30);
    d->toolSizeSlider->setMaximum(90);
    d->toolSizeSlider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    d->toolSizeSlider->setMinimumSize(QSize(80, 0));
    connect(d->toolSizeSlider, &QSlider::valueChanged, d, &ToolSettingsWidgetPrivate::sizeSliderMoved);

//    layout->addWidget(new QLabel(tr("Size:"), this));
    layout->addWidget(d->toolSizeSlider);
}

void ToolSettingsWidget::setCanvas(CanvasWidget *newCanvas)
{
    Q_D(ToolSettingsWidget);

    if (d->canvas)
        disconnect(d->canvas, 0, this, 0);

    d->canvas = newCanvas;

    if (d->canvas)
    {
        connect(d->canvas, SIGNAL(updateTool()), this, SLOT(updateTool()));
        connect(d->canvas, SIGNAL(destroyed(QObject *)), this, SLOT(canvasDestroyed(QObject *)));
    }

    updateTool();
}

void ToolSettingsWidget::canvasDestroyed(QObject *obj)
{
    Q_D(ToolSettingsWidget);
    d->canvas = NULL;
}

void ToolSettingsWidget::updateTool()
{
    Q_D(ToolSettingsWidget);
    d->freezeUpdates = true;

    d->toolColorDial->setColor(d->canvas->getToolColor());

    float sizeFactor = d->canvas->getToolSizeFactor();

    if (sizeFactor > 1.0f)
        d->toolSizeSlider->setValue((sizeFactor - 1.0f) * 10.0f);
    else if (sizeFactor < 1.0f)
        d->toolSizeSlider->setValue(-(((1.0f / sizeFactor) - 1.0f) * 10.0f));
    else
        d->toolSizeSlider->setValue(1.0f);

    d->freezeUpdates = false;
}
