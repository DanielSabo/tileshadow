#include "toolextendedsettingswindow.h"
#include "canvaswidget.h"
#include "maskbuffer.h"
#include "gbrfile.h"
#include "filedialog.h"
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
#include <QImageReader>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QMenu>

class ToolExtendedSettingsWindowPrivate
{
public:
    ToolExtendedSettingsWindowPrivate(CanvasWidget *canvas)
        : canvas(canvas), freezeUpdates(false) {}

    CanvasWidget *canvas;

    QWidget     *settingsAreaBody;
    QScrollArea *scroll;
    QPointer<QWidget> settingsAreaSettings;

    QWidget     *inputEditorBody;
    QPushButton *inputEditorApplyButton;
    QString      inputEditorSettingID;
    QPointer<QWidget> inputEditorSettings;

    PreviewWidget *previewWidget;
    bool needsPreview;

    QString maskSettingName;
    QString textureSettingName;

    QPushButton *textureButton;
    QAction *clearTextureAction;
    QAction *setTextureAction;
    QAction *exportTextureAction;

    QPushButton *masksButton;
    QAction *clearMasksAction;
    QAction *setMasksAction;
    QAction *exportMasksAction;

    QPushButton *saveButton;

    std::vector<CanvasStrokePoint> previewStrokeData;
    QPoint previewStrokeCenter;
    QString activeToolpath;
    bool freezeUpdates;

    struct SettingItem
    {
        QString settingID;
        QString settingName;
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

    void hideMappings();
    void showMappingsFor(ToolSettingInfo const &info);
    void applyMappings();

    void saveToolAs();

    void clearMasks();
    void importMasks();
    void exportMasks();

    void clearTexture();
    void importTexture();
    void exportTexture();

    void loadPreviewStroke(QString const &path);
    void setPreviewStrokeData(std::vector<CanvasStrokePoint> &&data);
    void updatePreview();
};

ToolExtendedSettingsWindow::ToolExtendedSettingsWindow(CanvasWidget *canvas, QWidget *parent)
    : QWidget(parent, Qt::Dialog),
      d_ptr(new ToolExtendedSettingsWindowPrivate(canvas))
{
    Q_D(ToolExtendedSettingsWindow);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);

    auto windowBody = new QWidget();
    auto windowHorizontal = new QHBoxLayout(windowBody);
    windowHorizontal->setSpacing(3);
    layout->addWidget(windowBody);

    auto rightBody = new QWidget();
    auto rightVertical = new QVBoxLayout(rightBody);
    rightVertical->setSpacing(3);
    rightVertical->setContentsMargins(QMargins());
    rightBody->setFixedWidth(600);

    d->scroll = new QScrollArea();
    d->scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    d->scroll->setWidgetResizable(true);
    d->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->scroll->setFrameShape(QFrame::NoFrame);

    d->settingsAreaBody = new QWidget();
    auto settingsAreaBodyLayout = new QVBoxLayout(d->settingsAreaBody);
    settingsAreaBodyLayout->setContentsMargins(QMargins());
    settingsAreaBodyLayout->setSpacing(0);
    d->settingsAreaSettings = nullptr;

    d->scroll->setWidget(d->settingsAreaBody);
    windowHorizontal->addWidget(d->scroll);
    windowHorizontal->addWidget(rightBody);

    auto previewScroll = new QScrollArea();
    d->previewWidget = new PreviewWidget();
    d->previewWidget->setImage(QImage(), Qt::white);
    previewScroll->setWidgetResizable(true);
    previewScroll->setWidget(d->previewWidget);

    auto previewVerticalLayout = new QVBoxLayout();
    auto previewButtonArea = new QHBoxLayout();
    previewVerticalLayout->addWidget(previewScroll);
    previewVerticalLayout->addLayout(previewButtonArea);
    previewVerticalLayout->setStretch(0, 1);
    auto previewDefaultStrokeButton = new QPushButton(tr("Reset Stroke"));
    auto previewCopyStrokeButton = new QPushButton(tr("Copy Last Stroke"));
    previewButtonArea->addStretch(1);
    previewButtonArea->addWidget(previewDefaultStrokeButton);
    previewButtonArea->addWidget(previewCopyStrokeButton);

    connect(previewDefaultStrokeButton, &QPushButton::clicked, this, [d](bool) {
        if (d->canvas)
        {
            d->loadPreviewStroke(":/preview_strokes/default.json");
            d->updatePreview();
        }
    });

    connect(previewCopyStrokeButton, &QPushButton::clicked, this, [d](bool) {
        if (d->canvas)
        {
            auto lastStroke = d->canvas->getLastStrokeData();
            if (!lastStroke.empty())
            {
                d->setPreviewStrokeData(std::move(lastStroke));
                d->updatePreview();
            }
        }
    });

    auto inputEditorArea = new QWidget();
    inputEditorArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto inputEditorAreaLayout = new QVBoxLayout(inputEditorArea);
    inputEditorAreaLayout->setContentsMargins(QMargins());
    inputEditorAreaLayout->setSpacing(0);

    rightVertical->addLayout(previewVerticalLayout);
    rightVertical->addWidget(inputEditorArea);
    rightVertical->setStretch(0, 1);
    rightVertical->setStretch(1, 1);

    d->inputEditorBody = new QWidget();
    auto inputEditorBodyLayout = new QVBoxLayout(d->inputEditorBody);
    inputEditorBodyLayout->setContentsMargins(QMargins());
    inputEditorBodyLayout->setSpacing(0);
    d->inputEditorSettings = nullptr;

    auto inputEditorButtons = new QWidget();
    auto inputEditorButtonsLayout = new QHBoxLayout(inputEditorButtons);
    inputEditorButtonsLayout->setContentsMargins(QMargins());
    inputEditorAreaLayout->addStretch(1);
    inputEditorAreaLayout->addWidget(d->inputEditorBody);
    inputEditorAreaLayout->addWidget(inputEditorButtons);

    d->inputEditorApplyButton = new QPushButton(tr("Apply"));
    connect(d->inputEditorApplyButton, &QPushButton::clicked, this, [d](bool) {
        d->applyMappings();
    });
    inputEditorButtonsLayout->addStretch(1);
    inputEditorButtonsLayout->addWidget(d->inputEditorApplyButton);

    inputEditorArea->setStyleSheet(QStringLiteral(
        "QLineEdit[valid=\"false\"] {"
        "color: red;"
        "}"
    ));

    auto buttonsBox = new QWidget();
    auto buttonsLayout = new QHBoxLayout(buttonsBox);

    d->saveButton = new QPushButton(tr("Save As..."));
    connect(d->saveButton, &QPushButton::clicked, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->saveToolAs();
    });

    d->textureButton = new QPushButton(tr("Brush Texture"));
    QMenu *textureMenu = new QMenu();
    d->textureButton->setMenu(textureMenu);

    d->clearTextureAction = textureMenu->addAction(tr("Clear Texture"));
    connect(d->clearTextureAction, &QAction::triggered, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->clearTexture();
    });

    d->setTextureAction = textureMenu->addAction(tr("Import Texture..."));
    connect(d->setTextureAction, &QAction::triggered, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->importTexture();
    });

    d->exportTextureAction = textureMenu->addAction(tr("Export Texture..."));
    connect(d->exportTextureAction, &QAction::triggered, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->exportTexture();
    });

    d->masksButton = new QPushButton(tr("Brush Mask"));
    QMenu *maskMenu = new QMenu();
    d->masksButton->setMenu(maskMenu);

    d->clearMasksAction = maskMenu->addAction(tr("Clear Masks"));
    connect(d->clearMasksAction, &QAction::triggered, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->clearMasks();
    });

    d->setMasksAction = maskMenu->addAction(tr("Import Masks..."));
    connect(d->setMasksAction, &QAction::triggered, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->importMasks();
    });

    d->exportMasksAction = maskMenu->addAction(tr("Export Masks..."));
    connect(d->exportMasksAction, &QAction::triggered, this, [this](bool) {
        Q_D(ToolExtendedSettingsWindow);
        if (d->canvas)
            d->exportMasks();
    });

    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(d->textureButton);
    buttonsLayout->addWidget(d->masksButton);
    buttonsLayout->addWidget(d->saveButton);

    layout->addWidget(buttonsBox);

    d->loadPreviewStroke(":/preview_strokes/default.json");
    d->needsPreview = true;

    QSettings appSettings;

    if (appSettings.contains("ToolExtendedSettingsWindow/geometry"))
        restoreGeometry(appSettings.value("ToolExtendedSettingsWindow/geometry").toByteArray());

    setWindowTitle(tr("Tool Settings"));


    connect(d->canvas, &CanvasWidget::updateTool, this, &ToolExtendedSettingsWindow::updateTool);
    updateTool();
}

ToolExtendedSettingsWindow::~ToolExtendedSettingsWindow()
{
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

bool ToolExtendedSettingsWindow::event(QEvent *event)
{
    Q_D(ToolExtendedSettingsWindow);

    if ((event->type() == QEvent::WindowActivate) && d->needsPreview)
        d->updatePreview();

    return QWidget::event(event);
}

void ToolExtendedSettingsWindow::updateTool()
{
    Q_D(ToolExtendedSettingsWindow);
    d->freezeUpdates = true;

    QString canvasActiveTool = d->canvas->getActiveTool();

    if (d->activeToolpath != canvasActiveTool)
    {
        bool keepInputEditor = false;
        d->maskSettingName = QString();
        d->textureSettingName = QString();

        d->activeToolpath = canvasActiveTool;

        d->saveButton->setDisabled(d->canvas->getToolSaveable() == false);

        if (!d->settingsAreaSettings.isNull())
            delete d->settingsAreaSettings.data();
        d->items.clear();

        auto settingsBox = new QWidget();
        auto layout = new QGridLayout(settingsBox);
        layout->setSpacing(3);
        layout->setColumnStretch(0, 1);

        for (const ToolSettingInfo info: d->canvas->getAdvancedToolSettings())
        {
            if (info.type == ToolSettingInfoType::ExponentSlider || info.type == ToolSettingInfoType::LinearSlider)
            {
                QString settingID = info.settingID;

                QLabel *label = new QLabel(info.name);
                QPushButton *mappingButton = new QPushButton();
                if (!info.mapping.empty())
                {
                    if (settingID == d->inputEditorSettingID)
                    {
                        keepInputEditor = true;
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

                layout->addWidget(label, layout->rowCount(), 0, 1, 2, Qt::AlignLeft);
                layout->addWidget(slider, layout->rowCount(), 0);
                layout->addWidget(mappingButton, layout->rowCount() - 1, 1);

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
                });

                d->items.push_back({info.settingID, info.name, label, slider, mappingButton});
            }
            else if (info.type == ToolSettingInfoType::MaskSet)
            {
                d->maskSettingName = info.settingID;
            }
            else if (info.type == ToolSettingInfoType::Texture)
            {
                d->textureSettingName = info.settingID;
            }
        }

        d->settingsAreaSettings = settingsBox;
        d->settingsAreaBody->layout()->addWidget(settingsBox);

        d->scroll->setMinimumWidth(d->scroll->widget()->minimumSizeHint().width() +
                                   d->scroll->verticalScrollBar()->width());

        if (!keepInputEditor)
            d->hideMappings();
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

    if (!d->maskSettingName.isEmpty())
    {
        d->masksButton->setEnabled(true);
        bool hasMasks = !d->canvas->getToolSetting(d->maskSettingName).value<QList<MaskBuffer>>().empty();
        d->exportMasksAction->setEnabled(hasMasks);
        d->setMasksAction->setEnabled(true);
        d->clearMasksAction->setEnabled(hasMasks);
    }
    else
    {
        d->masksButton->setEnabled(false);
        d->exportMasksAction->setEnabled(false);
        d->setMasksAction->setEnabled(false);
        d->clearMasksAction->setEnabled(false);
    }

    if (!d->textureSettingName.isEmpty())
    {
        d->textureButton->setEnabled(true);
        bool hasTexture = !d->canvas->getToolSetting(d->textureSettingName).value<MaskBuffer>().isNull();
        d->setTextureAction->setEnabled(true);
        d->clearTextureAction->setEnabled(hasTexture);
        d->exportTextureAction->setEnabled(hasTexture);
    }
    else
    {
        d->textureButton->setEnabled(false);
        d->setTextureAction->setEnabled(false);
        d->clearTextureAction->setEnabled(false);
        d->exportTextureAction->setEnabled(false);
    }

    if (isActiveWindow())
        d->updatePreview();
    else
        d->needsPreview = true;

    d->freezeUpdates = false;
}

void ToolExtendedSettingsWindowPrivate::hideMappings()
{
    if (!inputEditorSettings.isNull())
        delete inputEditorSettings.data();
    inputEditorItems.clear();
    inputEditorApplyButton->hide();
}

void ToolExtendedSettingsWindowPrivate::showMappingsFor(ToolSettingInfo const &info)
{
    if (!inputEditorSettings.isNull())
        delete inputEditorSettings.data();
    inputEditorItems.clear();

    auto settingsBox = new QWidget();
    auto layout = new QFormLayout(settingsBox);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

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
    inputEditorApplyButton->show();
    inputEditorSettings = settingsBox;
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

void ToolExtendedSettingsWindowPrivate::saveToolAs()
{
    CanvasWidget::ToolSaveInfo saveInfo = canvas->serializeTool();

    if (saveInfo.fileExtension.isEmpty() || saveInfo.saveDirectory.isEmpty() || saveInfo.serialzedTool.isEmpty())
        return;

    QString defaultFilename = saveInfo.saveDirectory + QDir::toNativeSeparators("/") +
                              QObject::tr("untitled") + "." + saveInfo.fileExtension;

    QString filename = FileDialog::getSaveFileName(nullptr, QObject::tr("Save As..."),
                                                   defaultFilename,
                                                   QString("%1 (*.%1)").arg(saveInfo.fileExtension));

    if (filename.isEmpty())
        return;

    QFile outfile(filename);
    outfile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    outfile.write(saveInfo.serialzedTool);
    outfile.close();
}

void ToolExtendedSettingsWindowPrivate::clearMasks()
{
    if (maskSettingName.isEmpty())
        return;

    canvas->setToolSetting(maskSettingName, QVariant::fromValue(QList<MaskBuffer>()));
}

void ToolExtendedSettingsWindowPrivate::importMasks()
{
    if (maskSettingName.isEmpty())
        return;

    QStringList importWildcards;

    for (auto const &readerFormat: QImageReader::supportedImageFormats())
        importWildcards.append(QStringLiteral("*.") + readerFormat);
    importWildcards.append(QStringLiteral("*.gbr"));
    importWildcards.append(QStringLiteral("*.gih"));

    QString formats = QObject::tr("Images") + " (" + importWildcards.join(" ") + ")";

    QStringList filenames = FileDialog::getOpenFileNames(nullptr,
                                                         QObject::tr("Import Masks"),
                                                         QDir::homePath(),
                                                         formats);

    if (filenames.isEmpty())
        return;

    QList<MaskBuffer> masks;

    for (auto const &filename: filenames)
    {
        if (filename.endsWith(".gbr"))
        {
            QFile file(filename);
            QImage image = readGBR(&file);
            if (!image.isNull())
            {
                MaskBuffer mask(image);
                masks.append(mask);
            }
        }
        else if (filename.endsWith(".gih"))
        {
            QFile file(filename);
            QList<QImage> images = readGIH(&file);
            for (auto const &image: images)
            {
                if (!image.isNull())
                {
                    MaskBuffer mask(image);
                    masks.append(mask);
                }
            }
        }
        else
        {
            QImage image(filename);
            if (!image.isNull())
            {
                MaskBuffer mask(image);
                mask = mask.invert();
                masks.append(mask);
            }
        }
    }

    canvas->setToolSetting(maskSettingName, QVariant::fromValue(masks));
}

void ToolExtendedSettingsWindowPrivate::exportMasks()
{
    if (maskSettingName.isEmpty())
        return;

    QList<MaskBuffer> masks = canvas->getToolSetting(maskSettingName).value<QList<MaskBuffer>>();

    if (masks.empty())
        return;

    QString exportPath = FileDialog::getSaveFileName(nullptr,
                                                     QObject::tr("Export Masks"),
                                                     QDir::homePath());

    if (exportPath.isEmpty())
        return;

    if (QFileInfo::exists(exportPath))
    {
        QMessageBox::warning(nullptr,
                             QObject::tr("Export path already exists"),
                             QObject::tr("Export can't overwrite files or directores, please pick a different file name."));
        return;
    }

    QDir exportDirectory(exportPath);
    exportDirectory.mkpath(exportDirectory.absolutePath());

    int index = 1;
    for (auto const &mask: masks)
    {
        QString exportFilename = QObject::tr("mask") + QString("-%1.png").arg(index++, 2, 10, QChar('0'));
        QFile exportFile(exportDirectory.absoluteFilePath(exportFilename));
        exportFile.open(QIODevice::WriteOnly);
        exportFile.write(mask.invert().toPNG());
        exportFile.close();
    }
}

void ToolExtendedSettingsWindowPrivate::clearTexture()
{
    if (textureSettingName.isEmpty())
        return;

    canvas->setToolSetting(textureSettingName, QVariant::fromValue(MaskBuffer()));
}

void ToolExtendedSettingsWindowPrivate::importTexture()
{
    if (textureSettingName.isEmpty())
        return;

    QStringList importWildcards;

    for (auto const &readerFormat: QImageReader::supportedImageFormats())
        importWildcards.append(QStringLiteral("*.") + readerFormat);

    QString formats = QObject::tr("Images") + " (" + importWildcards.join(" ") + ")";

    QString filename = FileDialog::getOpenFileName(nullptr,
                                                   QObject::tr("Import Texture"),
                                                   QDir::homePath(),
                                                   formats);

    if (filename.isNull())
        return;

    QImage image(filename);
    if (!image.isNull())
    {
        MaskBuffer mask(image);
        // Note: Unlike brush masks, textures are not inverted
        canvas->setToolSetting(textureSettingName, QVariant::fromValue(mask));
    }
}

void ToolExtendedSettingsWindowPrivate::exportTexture()
{
    if (textureSettingName.isEmpty())
        return;

    MaskBuffer mask = canvas->getToolSetting(textureSettingName).value<MaskBuffer>();

    if (mask.isNull())
        return;

    QString exportPath = FileDialog::getSaveFileName(nullptr,
                                                     QObject::tr("Export Texture"),
                                                     QDir::homePath());

    if (exportPath.isEmpty())
        return;

    QFile exportFile(exportPath);
    exportFile.open(QIODevice::WriteOnly);
    exportFile.write(mask.toPNG());
    exportFile.close();
}

void ToolExtendedSettingsWindowPrivate::loadPreviewStroke(const QString &path)
{
    setPreviewStrokeData(CanvasStrokePoint::pointsFromFile(path));
}

void ToolExtendedSettingsWindowPrivate::setPreviewStrokeData(std::vector<CanvasStrokePoint> &&data)
{
    previewStrokeData = data;

    auto iter = previewStrokeData.begin();
    if (iter != previewStrokeData.end())
    {
        int x0, x1, y0, y1;
        x0 = x1 = iter->x;
        y0 = y1 = iter->y;

        while (++iter != previewStrokeData.end())
        {
            if (x0 > iter->x) { x0 = iter->x; }
            else if (x1 < iter->x) { x1 = iter->x; }

            if (y0 > iter->y) { y0 = iter->y; }
            else if (y1 < iter->y) { y1 = iter->y; }
        }
        previewStrokeCenter = {(x0 + x1) / 2, (y0 + y1) / 2};
    }
    else
    {
        previewStrokeCenter = {0, 0};
    }
}

void ToolExtendedSettingsWindowPrivate::updatePreview()
{
    QImage preview = canvas->previewTool(previewStrokeData);
    preview.setOffset(preview.rect().center() - previewStrokeCenter - preview.offset());
    previewWidget->setImage(preview, Qt::white);
    needsPreview = false;
}

PreviewWidget::PreviewWidget(QWidget *parent) :
    QWidget(parent)
{
}

QSize PreviewWidget::sizeHint() const
{
    return image.size();
}

void PreviewWidget::setImage(const QImage &image, const QColor &background)
{
    this->image = image;
    this->background = background;

    update();
}

void PreviewWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    QSize widgetArea = size();
    QSize imageArea = image.size();
    QPoint origin = {
        widgetArea.width() / 2 - imageArea.width() / 2,
        widgetArea.height() / 2 - imageArea.height() / 2,
    };
    origin += image.offset();
    painter.fillRect(event->rect(), background);
    painter.drawImage(origin, image);
}
