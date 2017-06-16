#include "userpathsdialog.h"
#include "ui_userpathsdialog.h"
#include "toolfactory.h"

#include <QSettings>
#include <QDebug>

UserPathsDialog::UserPathsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UserPathsDialog)
{
    ui->setupUi(this);
    connect(this, &QDialog::accepted, this, &UserPathsDialog::savePaths);
}

UserPathsDialog::~UserPathsDialog()
{
}

void UserPathsDialog::savePaths()
{
    QSettings settings;
    QStringList toolPaths = ui->toolListWidget->getPaths();
    settings.setValue("Paths/UserTools", toolPaths);
    QStringList patternPaths = ui->fillPatternListWidget->getPaths();
    settings.setValue("Paths/UserPatterns", patternPaths);
}

void UserPathsDialog::showEvent(QShowEvent *event)
{
    QSettings settings;
    QStringList toolPaths = settings.value("Paths/UserTools").toStringList();
    ui->toolListWidget->setPaths({ToolFactory::getUserToolsPath()}, toolPaths);
    QStringList patternPaths = settings.value("Paths/UserPatterns").toStringList();
    ui->fillPatternListWidget->setPaths({ToolFactory::getUserPatternPath()}, patternPaths);
}
