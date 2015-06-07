#include "toolextendedsettingswindow.h"
#include "canvaswidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QCloseEvent>
#include <QSettings>
#include <QDebug>

class ToolExtendedSettingsWindowPrivate
{
public:
    ToolExtendedSettingsWindowPrivate(CanvasWidget *canvas)
        : canvas(canvas), freezeUpdates(false) {}

    QVBoxLayout *settingsWidgetLayout;
    QScrollArea *scroll;
    CanvasWidget *canvas;
    QString activeToolpath;
    bool freezeUpdates;

    struct SettingItem
    {
        QString settingID;
//        ToolSettingInfoType::Type type;
        QString settingName;
        QLabel *label;
        QWidget *input;
    };

    QList<SettingItem> items;

    void removeSettings();
    void addSetting(const ToolSettingInfo &info);
};

ToolExtendedSettingsWindow::ToolExtendedSettingsWindow(CanvasWidget *canvas, QWidget *parent)
    : QWidget(parent, Qt::Tool),
      d_ptr(new ToolExtendedSettingsWindowPrivate(canvas))
{
    Q_D(ToolExtendedSettingsWindow);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(QMargins());
    setLayout(layout);

    d->scroll = new QScrollArea();
    d->scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    d->scroll->setWidgetResizable(true);
    d->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->scroll->setFrameShape(QFrame::NoFrame);

    QWidget *settingsArea = new QWidget();
    d->scroll->setWidget(settingsArea);

    d->settingsWidgetLayout = new QVBoxLayout();
    d->settingsWidgetLayout->setSpacing(3);
    settingsArea->setLayout(d->settingsWidgetLayout);
    layout->addWidget(d->scroll);

    QSettings appSettings;

    if (appSettings.contains("ToolExtendedSettingsWindow/geometry"))
        restoreGeometry(appSettings.value("ToolExtendedSettingsWindow/geometry").toByteArray());

    setWindowTitle(tr("Tool Settings"));

    connect(d->canvas, &CanvasWidget::updateTool, this, &ToolExtendedSettingsWindow::updateTool);
    updateTool();
}

void ToolExtendedSettingsWindow::closeEvent(QCloseEvent *closeEvent)
{
    QSettings().setValue("ToolExtendedSettingsWindow/geometry", saveGeometry());
    closeEvent->accept();
}

void ToolExtendedSettingsWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    if (!keyEvent->modifiers() && keyEvent->key() == Qt::Key_Escape)
        close();
    else
        keyEvent->ignore();
}

void ToolExtendedSettingsWindow::updateTool()
{
    Q_D(ToolExtendedSettingsWindow);
    d->freezeUpdates = true;

    QString canvasActiveTool = d->canvas->getActiveTool();

    if (d->activeToolpath != canvasActiveTool)
    {
        d->activeToolpath = canvasActiveTool;

        d->removeSettings();
        for (const ToolSettingInfo &info: d->canvas->getAdvancedToolSettings())
        {
            QString settingID = info.settingID;

            QLabel *label = new QLabel(info.name);
            d->settingsWidgetLayout->addWidget(label);
            QSlider *slider = new QSlider(Qt::Horizontal);
            d->settingsWidgetLayout->addWidget(slider);
            slider->setMinimum(info.min * 100);
            slider->setMaximum(info.max * 100);
            slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

            connect(slider, &QSlider::valueChanged, this, [d, settingID] (int value) {
                if (!d->freezeUpdates)
                {
                    d->freezeUpdates = true;
                    d->canvas->setToolSetting(settingID, QVariant::fromValue<float>(value / 100.0f));
                    d->freezeUpdates = false;
                }
            });

            d->items.push_back({info.settingID, info.name, label, slider});
        }

        d->scroll->setMinimumWidth(d->scroll->widget()->minimumSizeHint().width() +
                                   d->scroll->verticalScrollBar()->width());
    }

    for (auto &iter: d->items)
    {
        float value = d->canvas->getToolSetting(iter.settingID).toFloat();
        iter.label->setText(QStringLiteral("%1: %2").arg(iter.settingName).arg(value, 0, 'f', 2));
        if (QSlider *slider = qobject_cast<QSlider *>(iter.input))
        {
            slider->setValue(value * 100);
        }
    }

    d->freezeUpdates = false;
}

void ToolExtendedSettingsWindowPrivate::removeSettings()
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
