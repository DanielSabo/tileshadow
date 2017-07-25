#include "toolsettingswidget.h"

#include "canvaswidget.h"
#include "hsvcolordial.h"
#include "toolsettinginfo.h"
#include <qmath.h>
#include <QSlider>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>

#include <QComboBox>
#include <QDebug>

class ToolSettingsWidgetPrivate
{
public:
    ToolSettingsWidgetPrivate(ToolSettingsWidget *q, CanvasWidget *canvas);

    ToolSettingsWidget * const q_ptr;
    Q_DECLARE_PUBLIC(ToolSettingsWidget)

    CanvasWidget *canvas;
    HSVColorDial *toolColorDial;

    bool freezeUpdates;

    void colorDialDrag(const QColor &color);
    void colorDialChanged(const QColor &color);

    template <typename T> void updateSetting(const QString &settingID, T value);
    void updateFloatSetting(const QString &settingID, float value);
    void updateBoolSetting(const QString &settingID, bool value);

    void addSetting(const ToolSettingInfo &info);
    void addSlider(const QString &settingID, ToolSettingInfoType::Type sliderType, const QString &name, float min, float max);
    void addCheckbox(const QString &settingID, const QString &name);
    void addListbox(const QString &settingID, const QString &name, const ToolSettingInfo::OptionsList &options);

    void removeSettings();

    struct SettingItem
    {
        QString settingID;
        ToolSettingInfoType::Type type;
        QLabel *label;
        QWidget *input;
    };

    QList<SettingItem> items;
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

template <typename T> void ToolSettingsWidgetPrivate::updateSetting(const QString &settingID, T value)
{
    if (freezeUpdates)
        return;

    freezeUpdates = true;
    canvas->setToolSetting(settingID, QVariant::fromValue<T>(value));
    freezeUpdates = false;
}


void ToolSettingsWidgetPrivate::addSlider(const QString &settingID, ToolSettingInfoType::Type sliderType, const QString &name, float min, float max)
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

    items.push_back({settingID, sliderType, label, slider});
}

void ToolSettingsWidgetPrivate::addCheckbox(const QString &settingID, const QString &name)
{
    Q_Q(ToolSettingsWidget);

    QCheckBox *check = new QCheckBox(name, q);
    q->layout()->addWidget(check);
    QObject::connect(check, &QCheckBox::stateChanged, [=] (int state) { updateBoolSetting(settingID, state == Qt::Checked); });

    items.push_back({settingID, ToolSettingInfoType::Checkbox, nullptr, check});
}

void ToolSettingsWidgetPrivate::addListbox(const QString &settingID, const QString &name, const ToolSettingInfo::OptionsList &options)
{
    Q_Q(ToolSettingsWidget);

    QComboBox *listbox = new QComboBox();
    listbox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    for (auto const &option: options)
        listbox->addItem(option.second, option.first);
    QLabel *label = new QLabel(name + ":");
    q->layout()->addWidget(label);
    q->layout()->addWidget(listbox);

    QObject::connect(listbox, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [=] (int row) {
        updateSetting<QString>(settingID, listbox->itemData(row).toString());
    });

    items.push_back({settingID, ToolSettingInfoType::Listbox, label, listbox});
}

void ToolSettingsWidgetPrivate::removeSettings()
{
    for (auto &iter: items)
    {
        if (iter.label)
            delete iter.label;
        if (iter.input)
            delete iter.input;
    }
    items.clear();
}

void ToolSettingsWidgetPrivate::addSetting(const ToolSettingInfo &info)
{
    if (info.type == ToolSettingInfoType::ExponentSlider ||
        info.type == ToolSettingInfoType::LinearSlider)
        addSlider(info.settingID, info.type, info.name, info.min, info.max);
    else if (info.type == ToolSettingInfoType::Checkbox)
        addCheckbox(info.settingID, info.name);
    else if (info.type == ToolSettingInfoType::Listbox)
        addListbox(info.settingID, info.name, info.options);
    else
        qDebug() << "Attempted to add unknown setting type" << info.type << info.settingID;
}

ToolSettingsWidget::ToolSettingsWidget(CanvasWidget *canvas, QWidget *parent) :
    QWidget(parent),
    d_ptr(new ToolSettingsWidgetPrivate(this, canvas))
{
    Q_D(ToolSettingsWidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(3);
    layout->setContentsMargins(12, 0, 12, 0);

    QHBoxLayout *dialLayout = new QHBoxLayout();
    dialLayout->setSpacing(0);
    dialLayout->setContentsMargins(0, 0, 0, 0);

    d->toolColorDial = new HSVColorDial(this);
    d->toolColorDial->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    d->toolColorDial->setMinimumSize(QSize(80, 80));
    d->toolColorDial->setBaseSize(QSize(80, 80));
    d->toolColorDial->setMaximumSize(QSize(180, 180));
    connect(d->toolColorDial, &HSVColorDial::dragColor, [=] (const QColor &color) { d->colorDialDrag(color); });
    connect(d->toolColorDial, &HSVColorDial::releaseColor, [=] (const QColor &color) { d->colorDialChanged(color); });
    dialLayout->addWidget(d->toolColorDial);
    layout->addLayout(dialLayout);

    connect(d->canvas, &CanvasWidget::updateTool, this, &ToolSettingsWidget::updateTool);
    updateTool(true);
}

ToolSettingsWidget::~ToolSettingsWidget()
{
}

void ToolSettingsWidget::updateTool(bool pathChangeOrReset)
{
    Q_D(ToolSettingsWidget);
    d->freezeUpdates = true;

    if (pathChangeOrReset)
    {
        d->removeSettings();
        for (const ToolSettingInfo &info: d->canvas->getToolSettings())
            d->addSetting(info);
    }

    d->toolColorDial->setColor(d->canvas->getToolColor());

    for (auto const &iter: d->items)
    {
        ToolSettingInfoType::Type type = iter.type;
        if (type == ToolSettingInfoType::ExponentSlider ||
            type == ToolSettingInfoType::LinearSlider)
        {
            bool ok;
            float value = d->canvas->getToolSetting(iter.settingID).toFloat(&ok);
            QSlider *input = qobject_cast<QSlider *>(iter.input);

            if (ok && input)
                input->setValue(value * 100.0f);
        }
        else if (type == ToolSettingInfoType::Checkbox)
        {
            bool value = d->canvas->getToolSetting(iter.settingID).toBool();
            QCheckBox *input = qobject_cast<QCheckBox *>(iter.input);

            if (input)
                input->setCheckState(value ? Qt::Checked : Qt::Unchecked);
        }
    }

    d->freezeUpdates = false;
}
