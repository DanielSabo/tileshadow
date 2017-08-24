#include "pathlistwidget.h"

#include <QFile>
#include <QDebug>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QMenu>
#include <QDesktopServices>
#include "filedialog.h"

class PathListWidgetPrivate
{
public:
    QTableWidget *listView = nullptr;
    QMenu *contextMenu;
    QAction *showAction;

    void addRow(PathListWidget *parent, const QString &text, bool editable = true);
    void validateItem(QTableWidgetItem *item);
};

PathListWidget::PathListWidget(QWidget *parent)
    : QWidget(parent),
      d_ptr(new PathListWidgetPrivate)
{
    Q_D(PathListWidget);

    d->contextMenu = new QMenu(this);
    d->showAction = d->contextMenu->addAction(tr("Show directory"));

    auto l = new QVBoxLayout(this);
    d->listView = new QTableWidget(this);
    d->listView->setColumnCount(3);
    d->listView->setRowCount(0);
    d->listView->verticalHeader()->hide();
    d->listView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    d->listView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    d->listView->horizontalHeader()->hide();
    l->setMargin(0);
    l->addWidget(d->listView);

    connect(d->listView, &QTableWidget::cellChanged, this, [this, d](int row, int column){
        if (column != 0)
            return;
        QTableWidgetItem *pathItem = d->listView->item(row, column);
        if (!pathItem)
            return;

        QSignalBlocker blocker(d->listView);

        bool lastRow = row == d->listView->rowCount() - 1;
        QString newText = pathItem->text();

        if (newText.isEmpty() && !lastRow)
            d->listView->removeRow(d->listView->row(pathItem));
        else
            d->validateItem(pathItem);

        if (!newText.isEmpty() && lastRow)
        {
            if (QWidget *deleteButton = d->listView->cellWidget(row, 2))
                deleteButton->setEnabled(true);
            d->addRow(this, "");
        }

        blocker.unblock();
        changed();
    });

    d->listView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(d->listView, &QWidget::customContextMenuRequested, this, [this, d](QPoint pos){
        QTableWidgetItem *item = d->listView->itemAt(pos);

        if (!item)
            return;

        QString const text = item->text();
        d->showAction->setEnabled(QFileInfo(text).isDir());

        QAction const *result = d->contextMenu->exec(d->listView->mapToGlobal(pos));
        if (result == d->showAction)
            QDesktopServices::openUrl(QUrl::fromLocalFile(text));
    });

    d->addRow(this, "");
}

PathListWidget::~PathListWidget()
{
}

void PathListWidget::setPaths(QStringList staticPaths, QStringList paths)
{
    Q_D(PathListWidget);

    QSignalBlocker blocker(d->listView);
    paths.removeAll("");

    d->listView->clear();
    d->listView->setRowCount(0);
    for (auto const &path: staticPaths)
        d->addRow(this, path, false);
    for (auto const &path: paths)
        d->addRow(this, path);
    d->addRow(this, "");

    blocker.unblock();
    changed();
}

QStringList PathListWidget::getPaths()
{
    Q_D(PathListWidget);

    QStringList result;
    const int numRows = d->listView->rowCount() - 1;

    for (int i = 0; i < numRows; ++i)
        if (auto item = d->listView->item(i, 0))
            if (item->flags().testFlag(Qt::ItemIsEnabled))
                result.append(item->text());

    return result;
}

void PathListWidgetPrivate::addRow(PathListWidget *parent, QString const &text, bool editable)
{
    QSignalBlocker blocker(listView);

    int rowNum = listView->rowCount();
    listView->insertRow(rowNum);

    QTableWidgetItem *pathItem = new QTableWidgetItem(text);
    QPushButton *selectButton = new QPushButton(QWidget::tr("..."));
    selectButton->setToolTip(QWidget::tr("Select a different directory"));
    QPushButton *deleteButton = new QPushButton(QWidget::tr("Del"));
    deleteButton->setToolTip(QWidget::tr("Remove path"));

    QObject::connect(deleteButton, &QPushButton::clicked, parent, [this, parent, pathItem](bool){
        listView->removeRow(listView->row(pathItem));
        parent->changed();
    });

    QObject::connect(selectButton, &QPushButton::clicked, parent, [this, pathItem](bool){
        QString dir = FileDialog::getExistingDirectory(listView, QWidget::tr("Select Directory"), QDir::homePath());
        if (!dir.isEmpty())
            pathItem->setText(dir);
    });

    if (editable)
    {
        pathItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled);

        validateItem(pathItem);
        if (text.isEmpty())
            deleteButton->setEnabled(false);
    }
    else
    {
        pathItem->setFlags(Qt::ItemIsEditable);
        selectButton->setEnabled(false);
        deleteButton->setEnabled(false);
    }

    listView->setItem(rowNum, 0, pathItem);
    listView->setCellWidget(rowNum, 1, selectButton);
    listView->setCellWidget(rowNum, 2, deleteButton);
}

void PathListWidgetPrivate::validateItem(QTableWidgetItem *item)
{
    const QString text = item->text();

    if (text.isEmpty() || QFileInfo(text).isDir())
        item->setForeground(listView->palette().foreground());
    else
        item->setForeground(Qt::red);
}
