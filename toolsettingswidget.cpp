#include "toolsettingswidget.h"

#include "canvaswidget.h"
#include "hsvcolordial.h"
#include "toolsettinginfo.h"
#include <qmath.h>
#include <QSlider>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>

#include <QDebug>

class ToolSettingsWidgetPrivate
{
public:
    ToolSettingsWidgetPrivate(ToolSettingsWidget *q, CanvasWidget *canvas);

    ToolSettingsWidget * const q_ptr;
    Q_DECLARE_PUBLIC(ToolSettingsWidget)

    CanvasWidget *canvas;
    QString activeToolpath;
    HSVColorDial *toolColorDial;

    bool freezeUpdates;

    void colorDialDrag(const QColor &color);
    void colorDialChanged(const QColor &color);

    void updateFloatSetting(const QString &settingID, float value);
    void updateBoolSetting(const QString &settingID, bool value);
    void addExponentSlider(const QString &settingID, const QString &name, float min, float max);
    void addLinearSlider(const QString &settingID, const QString &name, float min, float max);
    void addCheckbox(const QString &settingID, const QString &name);

    void removeSettings();
    void addSetting(const ToolSettingInfo &info);

    struct SettingItem
    {
        ToolSettingInfoType::Type type;
        QString name;
        QLabel *label;
        QWidget *input;
    };

    QMap<QString, SettingItem> items;
};

ToolSettingsWidgetPrivate::ToolSettingsWidgetPrivate(ToolSettingsWidget *q, CanvasWidget *canvas)
    : q_ptr(q),
      canvas(canvas),
      freezeUpdates(false)
{
}

void ToolSettingsWidgetPrivate::colorDialDrag(const QColor &color)
{
    if (freezeUpdates)
        return;

    freezeUpdates = true;
    canvas->setToolColor(color);
    canvas->showColorPreview(color);
    freezeUpdates = false;
}

void ToolSettingsWidgetPrivate::colorDialChanged(const QColor &color)
{
    if (freezeUpdates)
        return;

    freezeUpdates = true;
    canvas->setToolColor(color);
    canvas->hideColorPreview();
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

void ToolSettingsWidgetPrivate::updateBoolSetting(const QString &settingID, bool value)
{
    if (freezeUpdates)
        return;

    freezeUpdates = true;
    canvas->setToolSetting(settingID, QVariant::fromValue<bool>(value));
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

    items[settingID] = {ToolSettingInfoType::ExponentSlider, name, label, slider};
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

    items[settingID] = {ToolSettingInfoType::LinearSlider, name, label, slider};
}

void ToolSettingsWidgetPrivate::addCheckbox(const QString &settingID, const QString &name)
{
    Q_Q(ToolSettingsWidget);

    QCheckBox *check = new QCheckBox(name, q);
    q->layout()->addWidget(check);
    QObject::connect(check, &QCheckBox::stateChanged, [=] (int state) { updateBoolSetting(settingID, state == Qt::Checked); });

    items[settingID] = {ToolSettingInfoType::Checkbox, name, nullptr, check};
}

void ToolSettingsWidgetPrivate::removeSettings()
{
    for (auto iter = items.begin();
              iter != items.end();
              iter = items.erase(iter))
    {
        if (iter.value().label)
            delete iter.value().label;
        if (iter.value().input)
            delete iter.value().input;
    }
}

void ToolSettingsWidgetPrivate::addSetting(const ToolSettingInfo &info)
{
    if (info.type == ToolSettingInfoType::ExponentSlider)
        addExponentSlider(info.settingID, info.name, info.min, info.max);
    else if (info.type == ToolSettingInfoType::LinearSlider)
        addLinearSlider(info.settingID, info.name, info.min, info.max);
    else if (info.type == ToolSettingInfoType::Checkbox)
        addCheckbox(info.settingID, info.name);
    else
        qDebug() << "Attempted to add unknown setting type" << info.type << info.settingID;
}

ToolSettingsWidget::ToolSettingsWidget(CanvasWidget *canvas, QWidget *parent) :
    QWidget(parent),
    d_ptr(new ToolSettingsWidgetPrivate(this, canvas))
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
    connect(d->toolColorDial, &HSVColorDial::dragColor, [=] (const QColor &color) { d->colorDialDrag(color); });
    connect(d->toolColorDial, &HSVColorDial::releaseColor, [=] (const QColor &color) { d->colorDialChanged(color); });
    layout->addWidget(d->toolColorDial);

    connect(d->canvas, &CanvasWidget::updateTool, this, &ToolSettingsWidget::updateTool);
    updateTool();
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
        ToolSettingInfoType::Type type = iter.value().type;
        if (type == ToolSettingInfoType::ExponentSlider ||
            type == ToolSettingInfoType::LinearSlider)
        {
            bool ok;
            float value = d->canvas->getToolSetting(iter.key()).toFloat(&ok);
            QSlider *input = qobject_cast<QSlider *>(iter.value().input);

            if (ok && input)
            {
                //FIXME: Don't hardcode 100.0f here
                input->setValue(value * 100.0f);
            }
        }
        else if (type == ToolSettingInfoType::Checkbox)
        {
            bool value = d->canvas->getToolSetting(iter.key()).toBool();
            QCheckBox *input = qobject_cast<QCheckBox *>(iter.value().input);

            if (input)
            {
                input->setCheckState(value ? Qt::Checked : Qt::Unchecked);
            }
        }
    }

    d->freezeUpdates = false;
}
