#include "toolsettingswidget.h"

#include "canvaswidget.h"
#include "hsvcolordial.h"
#include "toolsettinginfo.h"
#include "toolfactory.h"
#include "sidebarpopup.h"
#include "palettepopup.h"
#include "paletteview.h"
#include "toollistview.h"
#include <qmath.h>
#include <QSlider>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QDebug>
#include <QPushButton>

class ToolSettingsWidgetPrivate
{
public:
    ToolSettingsWidgetPrivate(ToolSettingsWidget *q, CanvasWidget *canvas);

    ToolSettingsWidget * const q_ptr;
    Q_DECLARE_PUBLIC(ToolSettingsWidget)

    CanvasWidget *canvas;
    HSVColorDial *toolColorDial = nullptr;
    PalettePopup *palettePopup = nullptr;
    SidebarPopup *paletteListPopup = nullptr;
    ToolListView *paletteListView = nullptr;

    bool freezeUpdates;

    void colorDialDrag(const QColor &color);
    void colorDialChanged(const QColor &color);

    void setPalettePath(const QString &path);

    template <typename T> void updateSetting(const QString &settingID, T value);

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

void ToolSettingsWidgetPrivate::setPalettePath(const QString &path)
{
    Q_Q(ToolSettingsWidget);

    QFile file(path);
    palettePopup->setColorList(path, readGPL(&file));
    if (palettePopup->isVisible())
        palettePopup->reposition(q, toolColorDial);
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
    QObject::connect(slider, &QSlider::valueChanged, [=] (int value) { updateSetting<float>(settingID, float(value) / 100.0f); });

    items.push_back({settingID, sliderType, label, slider});
}

void ToolSettingsWidgetPrivate::addCheckbox(const QString &settingID, const QString &name)
{
    Q_Q(ToolSettingsWidget);

    QCheckBox *check = new QCheckBox(name, q);
    q->layout()->addWidget(check);
    QObject::connect(check, &QCheckBox::stateChanged, [=] (int state) { updateSetting<bool>(settingID, state == Qt::Checked); });

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
    d->toolColorDial->installEventFilter(this);
    connect(d->toolColorDial, &HSVColorDial::dragColor, [=] (const QColor &color) { d->colorDialDrag(color); });
    connect(d->toolColorDial, &HSVColorDial::releaseColor, [=] (const QColor &color) { d->colorDialChanged(color); });
    dialLayout->addWidget(d->toolColorDial);
    layout->addLayout(dialLayout);

    d->palettePopup = new PalettePopup(this);
    d->setPalettePath(ToolFactory::defaultPaletteName());
    connect(d->palettePopup, &PalettePopup::colorHover, this, [d] (const QColor &color) { d->colorDialDrag(color); });
    connect(d->palettePopup, &PalettePopup::colorSelect, this, [d] (const QColor &color) { d->colorDialChanged(color); });
    connect(d->palettePopup, &PalettePopup::promptPalette, this, [this, d] {
        d->palettePopup->hide();
        d->paletteListView->setToolList(ToolFactory::listPalettes());
        d->paletteListPopup->reposition(this, d->toolColorDial);

    });

    d->paletteListPopup = new SidebarPopup(this);
    QVBoxLayout *paletteListLayout = new QVBoxLayout(d->paletteListPopup);
    paletteListLayout->setSpacing(3);
    paletteListLayout->setContentsMargins(1, 1, 1, 1);
    d->paletteListView = new ToolListView(d->paletteListPopup);
    paletteListLayout->addWidget(d->paletteListView);
    connect(d->paletteListView, &ToolListView::selectionChanged, this, [this, d] (QString const &path) {
       d->paletteListPopup->hide();
       d->setPalettePath(path);
       d->palettePopup->reposition(this, d->toolColorDial);
    });

    connect(d->canvas, &CanvasWidget::updateTool, this, &ToolSettingsWidget::updateTool);
    updateTool(true);
}

ToolSettingsWidget::~ToolSettingsWidget()
{
}

bool ToolSettingsWidget::eventFilter(QObject *watched, QEvent *event)
{
    Q_D(ToolSettingsWidget);

    if (watched == d->toolColorDial)
    {
        if (event->type() == QEvent::MouseButtonPress &&
            static_cast<QMouseEvent *>(event)->button() != Qt::LeftButton)
        {
            if (static_cast<QMouseEvent *>(event)->button() == Qt::RightButton)
            {
                d->palettePopup->reposition(this, d->toolColorDial);
            }

            return true;
        }
        else if (event->type() == QEvent::MouseButtonRelease &&
                 static_cast<QMouseEvent *>(event)->button() != Qt::LeftButton)
        {
            return true;
        }
        else if (event->type() == QEvent::MouseButtonDblClick &&
                 static_cast<QMouseEvent *>(event)->button() != Qt::LeftButton)
        {
            return true;
        }
    }

    return false;
}

void ToolSettingsWidget::showPalettePopup()
{
    Q_D(ToolSettingsWidget);

    d->palettePopup->reposition(this, d->toolColorDial);
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

    QColor toolColor = d->canvas->getToolColor();
    d->toolColorDial->setColor(toolColor);
    d->palettePopup->setCurrentColor(toolColor);

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
