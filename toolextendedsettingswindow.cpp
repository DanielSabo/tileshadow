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
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QPointer>

class ToolExtendedSettingsWindowPrivate
{
public:
    ToolExtendedSettingsWindowPrivate(CanvasWidget *canvas)
        : canvas(canvas), freezeUpdates(false) {}

    QVBoxLayout *settingsWidgetLayout;
    QScrollArea *scroll;
    QPushButton *saveButton;
    CanvasWidget *canvas;

    QWidget     *inputEditorWindow;
    QWidget     *inputEditorBody;
    QPushButton *inputEditorApplyButton;
    QString      inputEditorSettingID;
    QPointer<QWidget> inputEditorSettings;

    QString activeToolpath;
    bool freezeUpdates;

    struct SettingItem
    {
        QString settingID;
        QString settingName;
        QWidget *box;
        QLabel *label;
        QWidget *input;
        QPushButton *mappingButton;
    };

    QList<SettingItem> items;

    struct InputEditorItem
    {
        QString inputID;
        QLineEdit *text;
    };

    QList<InputEditorItem> inputEditorItems;

    void removeSettings();
    void showMappingsFor(ToolSettingInfo const &info);
    void applyMappings();
};

ToolExtendedSettingsWindow::ToolExtendedSettingsWindow(CanvasWidget *canvas, QWidget *parent)
    : QWidget(parent, Qt::Tool),
      d_ptr(new ToolExtendedSettingsWindowPrivate(canvas))
{
    Q_D(ToolExtendedSettingsWindow);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
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

    d->inputEditorWindow = new QWidget(this, Qt::Tool);
    d->inputEditorWindow->setWindowTitle(tr("Inputs"));
    d->inputEditorWindow->setLayout(new QVBoxLayout());
    d->inputEditorWindow->layout()->setContentsMargins(QMargins());
    d->inputEditorWindow->layout()->setSpacing(0);

    d->inputEditorBody = new QWidget();
    d->inputEditorBody->setLayout(new QVBoxLayout());
    d->inputEditorBody->layout()->setContentsMargins(QMargins());
    d->inputEditorBody->layout()->setSpacing(0);
    d->inputEditorSettings = nullptr;

    QWidget *inputEditorButtons = new QWidget();
    QHBoxLayout *inputEditorButtonsLayout = new QHBoxLayout();
    inputEditorButtonsLayout->setContentsMargins(QMargins());
    inputEditorButtons->setLayout(inputEditorButtonsLayout);
    d->inputEditorWindow->layout()->addWidget(d->inputEditorBody);
    d->inputEditorWindow->layout()->addWidget(inputEditorButtons);

    d->inputEditorApplyButton = new QPushButton(tr("Apply"));
    connect(d->inputEditorApplyButton, &QPushButton::clicked, this, [d](bool) {
        d->applyMappings();
    });
    inputEditorButtonsLayout->addStretch(1);
    inputEditorButtonsLayout->addWidget(d->inputEditorApplyButton);

    d->inputEditorWindow->setStyleSheet(QStringLiteral(
        "QLineEdit[valid=\"false\"] {"
        "color: red;"
        "}"
    ));

    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    QWidget *buttonsBox = new QWidget();
    buttonsBox->setLayout(buttonsLayout);
    d->saveButton = new QPushButton(tr("Save As..."));
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(d->saveButton);
    connect(d->saveButton, &QPushButton::clicked, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->canvas->saveToolSettings();
    });
    layout->addWidget(buttonsBox);

    QSettings appSettings;

    if (appSettings.contains("ToolExtendedSettingsWindow/geometry"))
        restoreGeometry(appSettings.value("ToolExtendedSettingsWindow/geometry").toByteArray());

    //FIXME: This is an ugly hack around the window position getting lost on hide
    // For some reason under KDE this window will always get moved to the lower left corner unless
    // setGeometry has been called.
    d->inputEditorWindow->setGeometry(x(), y() + height() / 2, 600, 10);

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
        bool keepInputEditorOpen = false;

        d->saveButton->setDisabled(d->canvas->getToolSaveable() == false);
        d->removeSettings();
        for (const ToolSettingInfo info: d->canvas->getAdvancedToolSettings())
        {
            if (info.type == ToolSettingInfoType::ExponentSlider || info.type == ToolSettingInfoType::LinearSlider)
            {
                QString settingID = info.settingID;

                QWidget *box = new QWidget();
                auto boxLayout = new QVBoxLayout();
                boxLayout->setContentsMargins(QMargins());
                boxLayout->setSpacing(0);
                box->setLayout(boxLayout);
                QWidget *rowBox = new QWidget();
                auto rowLayout = new QHBoxLayout();
                rowLayout->setContentsMargins(QMargins());
                rowBox->setLayout(rowLayout);

                QLabel *label = new QLabel(info.name);
                QPushButton *mappingButton = new QPushButton();
                if (!info.mapping.empty())
                {
                    mappingButton->setText("<no>");
                    if (settingID == d->inputEditorSettingID)
                    {
                        keepInputEditorOpen = true;
                        d->showMappingsFor(info);
                    }
                }
                else
                {
                    mappingButton->setText("N/A");
                    mappingButton->setEnabled(false);
                }

                QSlider *slider = new QSlider(Qt::Horizontal);
                slider->setMinimum(info.min * 100);
                slider->setMaximum(info.max * 100);
                slider->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

                rowLayout->addWidget(label);
                rowLayout->addStretch(1);
                rowLayout->addWidget(mappingButton);
                boxLayout->addWidget(rowBox);
                boxLayout->addWidget(slider);

                connect(slider, &QSlider::valueChanged, this, [d, settingID] (int value) {
                    if (!d->freezeUpdates)
                    {
                        d->freezeUpdates = true;
                        d->canvas->setToolSetting(settingID, QVariant::fromValue<float>(value / 100.0f));
                        d->freezeUpdates = false;
                    }
                });

                connect(mappingButton, &QPushButton::clicked, this, [d, info] (bool) {
                    d->showMappingsFor(info);
                    d->inputEditorWindow->raise();
                    d->inputEditorWindow->activateWindow();
                });

                d->settingsWidgetLayout->addWidget(box);
                d->items.push_back({info.settingID, info.name, box, label, slider, mappingButton});
            }
        }

        d->scroll->setMinimumWidth(d->scroll->widget()->minimumSizeHint().width() +
                                   d->scroll->verticalScrollBar()->width());

        if (!keepInputEditorOpen)
            d->inputEditorWindow->hide();
    }

    for (auto &iter: d->items)
    {
        if (QSlider *slider = qobject_cast<QSlider *>(iter.input))
        {
            float value = d->canvas->getToolSetting(iter.settingID).toFloat();
            iter.label->setText(QStringLiteral("%1: %2").arg(iter.settingName).arg(value, 0, 'f', 2));
            slider->setValue(value * 100);
        }
        if (iter.mappingButton->isEnabled())
        {
            auto mappingList = d->canvas->getToolSetting(iter.settingID + ":mapping").value<QMap<QString, QList<QPointF>>>();
            bool hasMapping = false;
            for (auto const &mapping: mappingList)
                if (!mapping.isEmpty())
                    hasMapping = true;
            if (hasMapping)
                iter.mappingButton->setText("<yes>");
            else
                iter.mappingButton->setText("<no>");
        }
    }

    d->freezeUpdates = false;
}

void ToolExtendedSettingsWindowPrivate::removeSettings()
{
    for (auto &iter: items)
    {
        if (iter.box)
            delete iter.box;
    }
    items.clear();
}

void ToolExtendedSettingsWindowPrivate::showMappingsFor(ToolSettingInfo const &info)
{
    if (!inputEditorSettings.isNull())
        delete inputEditorSettings.data();
    inputEditorItems.clear();

    auto settingsBox = new QWidget();
    auto layout = new QFormLayout();
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    settingsBox->setLayout(layout);

    inputEditorWindow->setWindowTitle(QString(QObject::tr("Inputs for %1")).arg(info.name));
    inputEditorSettingID = info.settingID;

    auto mappingList = canvas->getToolSetting(info.settingID + ":mapping").value<QMap<QString, QList<QPointF>>>();
    for (auto const &iter: info.mapping)
    {
        QLineEdit *mappingEdit = new QLineEdit();

        QString text;
        for (QPointF point: mappingList.value(iter.first))
        {
            if (!text.isEmpty())
                text += " ";
            text += QStringLiteral("%1,%2").arg(point.x(), 0, 'f', 4).arg(point.y(), 0, 'f', 4);
        }
        mappingEdit->setText(text);

        layout->addRow(iter.second, mappingEdit);
        inputEditorItems.push_back({iter.first, mappingEdit});
    }

    inputEditorBody->layout()->addWidget(settingsBox);
    inputEditorSettings = settingsBox;
    inputEditorWindow->show();
}

void ToolExtendedSettingsWindowPrivate::applyMappings()
{
    auto mappingList = QMap<QString, QList<QPointF>>();

    bool ok = true;
    for (auto &iter: inputEditorItems)
    {
        QList<QPointF> points;
        QStringList textPoints = iter.text->text().split(" ", QString::SkipEmptyParts);
        for (auto &point: textPoints)
        {
            QStringList s = point.split(",");
            ok = s.length() == 2;
            if (!ok)
                break;
            float x = s.at(0).toFloat(&ok);
            if (!ok)
                break;
            float y = s.at(1).toFloat(&ok);
            if (!ok)
                break;
            points.push_back({x, y});
        }
        iter.text->setProperty("valid", ok);
        iter.text->setStyleSheet({}); // Force style recalculation
        if (!ok)
            break;
        mappingList[iter.inputID] = points;
    }

    if (ok)
        canvas->setToolSetting(inputEditorSettingID + ":mapping", QVariant::fromValue(mappingList));
}
