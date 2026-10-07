#include "file_list.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QtGlobal>
#include <cmath>

FileListWidget::FileListWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
    setAcceptDrops(true);
}

QString FileListWidget::buttonStyle() const
{
    return "QPushButton {"
           "    background-color: #007aff;"
           "    color: white;"
           "    border: none;"
           "    border-radius: 6px;"
           "    padding: 6px 12px;"
           "    font-size: 12px;"
           "    font-weight: 500;"
           "}"
           "QPushButton:hover { background-color: #0056cc; }"
           "QPushButton:pressed { background-color: #004499; }";
}

void FileListWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *headerWidget = new QWidget();
    headerWidget->setStyleSheet(
        "QWidget { background-color: #ffffff; border-bottom: 1px solid #d2d2d7; padding: 8px; }");
    auto *headerLayout = new QHBoxLayout(headerWidget);

    titleLabel_ = new QLabel("Files in Archive");
    titleLabel_->setStyleSheet(
        "QLabel { font-size: 14px; font-weight: bold; color: #1d1d1f; border: none; background: transparent; }");
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch();

    clearButton_ = new QPushButton("Clear All");
    connect(clearButton_, &QPushButton::clicked, this, &FileListWidget::clearFiles);
    clearButton_->setStyleSheet(buttonStyle());
    headerLayout->addWidget(clearButton_);

    layout->addWidget(headerWidget);

    tree_ = new QTreeWidget();
    tree_->setHeaderLabels({"Name", "Size", "Type", "Modified", "Path"});
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setSortingEnabled(true);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QHeaderView *header = tree_->header();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::Interactive);

    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QTreeWidget::customContextMenuRequested,
            this, &FileListWidget::showContextMenu);

    tree_->setStyleSheet(
        "QTreeWidget {"
        "    background-color: #ffffff; border: none; outline: none; color: #1d1d1f;"
        "    font-size: 13px; gridline-color: #f0f0f0;"
        "    selection-background-color: #007aff; selection-color: white; }"
        "QTreeWidget::item { padding: 6px 2px; border: none; }"
        "QTreeWidget::item:selected { background-color: #007aff; color: white; }"
        "QTreeWidget::item:hover { background-color: #f0f0f0; }"
        "QHeaderView::section {"
        "    background-color: #f5f5f7; color: #1d1d1f; border: none;"
        "    border-right: 1px solid #d2d2d7; padding: 8px 4px;"
        "    font-weight: bold; font-size: 12px; }"
        "QTreeWidget::branch { background: transparent; }");

    layout->addWidget(tree_);

    dropHint_ = new QLabel("Drop files here or use 'Add Files' to add to archive");
    dropHint_->setAlignment(Qt::AlignCenter);
    dropHint_->setStyleSheet(
        "QLabel { color: #86868b; font-size: 16px; padding: 40px;"
        "    border: 2px dashed #d2d2d7; border-radius: 8px; background-color: #fafafa; }");
    dropHint_->setVisible(false);
    layout->addWidget(dropHint_);

    updateDisplay();
}

void FileListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void FileListWidget::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls())
        if (url.isLocalFile())
            paths << url.toLocalFile();
    if (!paths.isEmpty()) {
        addFiles(paths);
        event->acceptProposedAction();
    }
}

void FileListWidget::addFiles(const QStringList &paths)
{
    for (const QString &path : paths) {
        QFileInfo fi(path);
        if (fi.isFile()) {
            if (!files_.contains(path)) {
                files_ << path;
                addFileItem(path);
            }
        } else if (fi.isDir()) {
            if (!files_.contains(path)) {
                files_ << path;
                addFolderItem(path);
            }
        }
    }
    updateDisplay();
    emit filesChanged(files_.size());
}

void FileListWidget::addFileItem(const QString &path)
{
    QFileInfo fi(path);
    QString type = fi.suffix().toUpper();
    if (type.isEmpty()) type = "File";
    auto *item = new QTreeWidgetItem(QStringList{
        fi.fileName(), formatSize(fi.size()), type,
        formatDate(fi.lastModified().toSecsSinceEpoch()), fi.absolutePath()});
    item->setData(0, Qt::UserRole, path);
    tree_->addTopLevelItem(item);
}

void FileListWidget::addFolderItem(const QString &path)
{
    QFileInfo fi(path);
    auto *item = new QTreeWidgetItem(QStringList{
        fi.fileName(), "Folder", "Folder",
        formatDate(fi.lastModified().toSecsSinceEpoch()), fi.absolutePath()});
    item->setData(0, Qt::UserRole, path);
    item->setText(0, QString("\U0001F4C1 ") + fi.fileName());
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    tree_->addTopLevelItem(item);
}

void FileListWidget::loadArchiveFiles(const std::vector<zipline::ArchiveEntry> &entries)
{
    clearFiles();
    for (const auto &e : entries) {
        auto *item = new QTreeWidgetItem(QStringList{
            QString::fromStdString(e.name), formatSize(e.size),
            QString::fromStdString(e.type), QString::fromStdString(e.modified),
            QString::fromStdString(e.path)});
        tree_->addTopLevelItem(item);
    }
    updateDisplay();
    emit filesChanged((int)entries.size());
}

void FileListWidget::clearFiles()
{
    files_.clear();
    tree_->clear();
    updateDisplay();
    emit filesChanged(0);
}

void FileListWidget::updateDisplay()
{
    bool hasFiles = tree_->topLevelItemCount() > 0;
    tree_->setVisible(hasFiles);
    dropHint_->setVisible(!hasFiles);
    if (hasFiles)
        titleLabel_->setText(QString("Files in Archive (%1)").arg(tree_->topLevelItemCount()));
    else
        titleLabel_->setText("Files in Archive");
}

void FileListWidget::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = tree_->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *removeAction = menu.addAction("Remove from Archive");
    connect(removeAction, &QAction::triggered, this, &FileListWidget::removeSelectedFiles);
    menu.addSeparator();
    QAction *infoAction = menu.addAction("Properties");
    connect(infoAction, &QAction::triggered, this, [this, item] { showFileInfo(item); });
    menu.exec(tree_->mapToGlobal(pos));
}

void FileListWidget::removeSelectedFiles()
{
    const auto selected = tree_->selectedItems();
    for (QTreeWidgetItem *item : selected) {
        QVariant data = item->data(0, Qt::UserRole);
        if (data.typeId() == QMetaType::QString) {
            QString p = data.toString();
            files_.removeAll(p);
        }
        int idx = tree_->indexOfTopLevelItem(item);
        if (idx >= 0)
            delete tree_->takeTopLevelItem(idx);
    }
    updateDisplay();
    emit filesChanged(files_.size());
}

void FileListWidget::showFileInfo(QTreeWidgetItem *item)
{
    QVariant data = item->data(0, Qt::UserRole);
    QString info;
    if (data.typeId() == QMetaType::QString) {
        QFileInfo fi(data.toString());
        QString type = fi.suffix().toUpper();
        if (type.isEmpty()) type = "File";
        info = QString("Name: %1\nSize: %2\nType: %3\nLocation: %4\nModified: %5")
                   .arg(fi.fileName(), formatSize(fi.size()), type, fi.absolutePath(),
                        formatDate(fi.lastModified().toSecsSinceEpoch()));
    } else {
        info = QString("Name: %1\nSize: %2\nType: %3")
                   .arg(item->text(0), item->text(1), item->text(2));
    }
    QMessageBox::information(this, "File Properties", info);
}

QString FileListWidget::formatSize(quint64 bytes)
{
    if (bytes == 0) return "0 B";
    static const char *names[] = {"B", "KB", "MB", "GB", "TB"};
    int i = (int)std::floor(std::log((double)bytes) / std::log(1024.0));
    if (i < 0) i = 0;
    if (i > 4) i = 4;
    double s = bytes / std::pow(1024.0, i);
    return QString::number(s, 'f', 2) + " " + names[i];
}

QString FileListWidget::formatDate(qint64 secs)
{
    return QDateTime::fromSecsSinceEpoch(secs).toString("yyyy-MM-dd hh:mm");
}
