#include "toolsettingswidget.h"

#include "canvaswidget.h"
#include "hsvcolordial.h"
#include "toolsettinginfo.h"
#include <qmath.h>
#include <QSlider>
#include <QVBoxLayout>
#include <QLabel>

#include <QDebug>

class ToolSettingsWidgetPrivate
{
public:
    ToolSettingsWidgetPrivate(ToolSettingsWidget *q);

    ToolSettingsWidget * const q_ptr;
    Q_DECLARE_PUBLIC(ToolSettingsWidget)

    CanvasWidget *canvas;
    QString activeToolpath;
    HSVColorDial *toolColorDial;

    bool freezeUpdates;

    void colorDialChanged(const QColor &color);

    void updateFloatSetting(const QString &settingID, float value);
    void addExponentSlider(const QString &settingID, const QString &name, float min, float max);
    void addLinearSlider(const QString &settingID, const QString &name, float min, float max);

    void removeSettings();
    void addSetting(const ToolSettingInfo &info);

    struct SettingItem
    {
        QString name;
        QLabel *label;
        QSlider *input;
    };

    QMap<QString, SettingItem> items;
};

ToolSettingsWidgetPrivate::ToolSettingsWidgetPrivate(ToolSettingsWidget *q)
    : q_ptr(q),
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

void ToolSettingsWidgetPrivate::updateFloatSetting(const QString &settingID, float value)
{
    if (freezeUpdates)
        return;

    freezeUpdates = true;
    canvas->setToolSetting(settingID, QVariant::fromValue<float>(value));
    freezeUpdates = false;
}

void ToolSettingsWidgetPrivate::addExponentSlider(const QString &settingID, const QString &name, float min, float max)
{
    Q_Q(ToolSettingsWidget);

    QSlider *slider = new QSlider(Qt::Horizontal, q);
    slider->setMinimum(min * 100);
    slider->setMaximum(max * 100);
    slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    slider->setMinimumSize(QSize(80, 0));
    slider->setToolTip(name);
    QLabel *label = new QLabel(name + ":", q);

    q->layout()->addWidget(label);
    q->layout()->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, [=] (int value) { updateFloatSetting(settingID, float(value) / 100.0f); });

    items[settingID] = {name, label, slider};
}

void ToolSettingsWidgetPrivate::addLinearSlider(const QString &settingID, const QString &name, float min, float max)
{
    Q_Q(ToolSettingsWidget);

    QSlider *slider = new QSlider(Qt::Horizontal, q);
    slider->setMinimum(min * 100);
    slider->setMaximum(max * 100);
    slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    slider->setMinimumSize(QSize(80, 0));
    slider->setToolTip(name);
    QLabel *label = new QLabel(name + ":", q);

    q->layout()->addWidget(label);
    q->layout()->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, [=] (int value) { updateFloatSetting(settingID, float(value) / 100.0f); });

    items[settingID] = {name, label, slider};
}

void ToolSettingsWidgetPrivate::removeSettings()
{
    for (auto iter = items.begin();
              iter != items.end();
              iter = items.erase(iter))
    {
        delete iter.value().label;
        delete iter.value().input;
    }
}

void ToolSettingsWidgetPrivate::addSetting(const ToolSettingInfo &info)
{
    if (info.type == ToolSettingInfoType::ExponentSlider)
        addExponentSlider(info.settingID, info.name, info.min, info.max);
    else if (info.type == ToolSettingInfoType::LinearSlider)
        addLinearSlider(info.settingID, info.name, info.min, info.max);
    else
        qDebug() << "Attempted to add unknown setting type" << info.type << info.settingID;
}

ToolSettingsWidget::ToolSettingsWidget(QWidget *parent) :
    QWidget(parent),
    d_ptr(new ToolSettingsWidgetPrivate(this))
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
    connect(d->toolColorDial, &HSVColorDial::updateColor, [=] (const QColor &color) { d->colorDialChanged(color); });
    layout->addWidget(d->toolColorDial);
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

    if (d->activeToolpath != d->canvas->getActiveTool())
    {
        d->activeToolpath = d->canvas->getActiveTool();

        QList<ToolSettingInfo> infos = d->canvas->getToolSettings();

        d->removeSettings();
        for (auto iter = infos.begin(); iter != infos.end(); ++iter)
            d->addSetting(*iter);
    }

    d->toolColorDial->setColor(d->canvas->getToolColor());

    for (auto iter = d->items.begin(); iter != d->items.end(); ++iter)
    {
        //FIXME: Don't assume everything is a float and a QSlider
        QVariant value = d->canvas->getToolSetting(iter.key());
        if (value.canConvert<float>())
        {
            //FIXME: Don't hardcode 100.0f here
            iter.value().input->setValue(value.toFloat() * 100.0f);
        }
    }

    d->freezeUpdates = false;
}
