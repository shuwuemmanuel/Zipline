/* Main Window for Zipline Archive Manager (C++ port of ui/main_window.py) */
#ifndef ZIPLINE_MAIN_WINDOW_H
#define ZIPLINE_MAIN_WINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QLabel>
#include <QProgressBar>
#include <QStatusBar>
#include <memory>

#include "../core/archive_manager.h"
#include "file_list.h"
#include "archive_settings.h"
#include "toolbar.h"
#include "menu_bar.h"

class ArchiveCreationWorker : public QThread {
    Q_OBJECT
public:
    ArchiveCreationWorker(zipline::ArchiveManager *manager,
                          QStringList files, QString outputPath,
                          zipline::ArchiveSettings settings)
        : manager_(manager), files_(std::move(files)),
          outputPath_(std::move(outputPath)), settings_(std::move(settings)) {}

signals:
    void progressUpdate(int percentage);
    void finishedSignal(bool success, QString message);

protected:
    void run() override;

private:
    zipline::ArchiveManager *manager_;
    QStringList files_;
    QString outputPath_;
    zipline::ArchiveSettings settings_;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void newArchive();
    void openArchive();
    void addFiles();
    void addFolder();
    void addFilesOrFolders();
    void extractAll();
    void createArchive(const zipline::ArchiveSettings &settings);
    void onArchiveProgress(int percentage);
    void onArchiveFinished(bool success, const QString &message);
    void showSettings();
    void showAbout();
    void updateFileCount(int count);
    void testCurrentArchive();

private:
    void setupUi();
    void setupStatusBar();
    void setupConnections();
    void applyAppleStyle();
    void setWindowIconFromAssets();
    void loadArchive(const QString &path);
    void extractArchiveTo(const QString &extractPath);

    zipline::ArchiveManager manager_;
    QString currentArchivePath_;

    MainMenuBar *menuBar_ = nullptr;
    MainToolBar *toolbar_ = nullptr;
    FileListWidget *fileList_ = nullptr;
    ArchiveSettingsWidget *settingsPanel_ = nullptr;

    QStatusBar *statusBar_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QLabel *fileCountLabel_ = nullptr;

    std::unique_ptr<ArchiveCreationWorker> worker_;
};

#endif
