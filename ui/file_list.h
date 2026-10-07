/* File List Widget for Zipline Archive Manager (C++ port of ui/file_list.py) */
#ifndef ZIPLINE_FILE_LIST_H
#define ZIPLINE_FILE_LIST_H

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <vector>

#include "../core/archive_manager.h"

class FileListWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileListWidget(QWidget *parent = nullptr);

    void addFiles(const QStringList &paths);
    void loadArchiveFiles(const std::vector<zipline::ArchiveEntry> &entries);
    void clearFiles();
    QStringList files() const { return files_; }

signals:
    void filesChanged(int count);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void showContextMenu(const QPoint &pos);
    void removeSelectedFiles();

private:
    void setupUi();
    void addFileItem(const QString &path);
    void addFolderItem(const QString &path);
    void updateDisplay();
    void showFileInfo(QTreeWidgetItem *item);
    static QString formatSize(quint64 bytes);
    static QString formatDate(qint64 secs);
    QString buttonStyle() const;

    QStringList files_;
    QTreeWidget *tree_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QLabel *dropHint_ = nullptr;
    QPushButton *clearButton_ = nullptr;
};

#endif
