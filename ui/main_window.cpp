#include "main_window.h"
#include "exe_settings_dialog.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QIcon>
#include <QPixmap>
#include <QApplication>
#include <QFileInfo>
#include <QFile>

void ArchiveCreationWorker::run()
{
    try {
        std::vector<std::string> files;
        for (const QString &f : files_) files.push_back(f.toStdString());
        manager_->createArchive(files, outputPath_.toStdString(), settings_,
                                [this](double pct) { emit progressUpdate((int)pct); });
        emit finishedSignal(true, "Archive created: " + QFileInfo(outputPath_).fileName());
    } catch (const std::exception &e) {
        emit finishedSignal(false, QString("Failed to create archive: ") + e.what());
    }
}

MainWindow::MainWindow()
{
    setWindowTitle("Zipline Archive Manager");
    setMinimumSize(600, 400);
    resize(700, 450);
    setWindowFlag(Qt::WindowFullscreenButtonHint, true);

    setupUi();
    setupConnections();
    applyAppleStyle();
    setWindowIconFromAssets();
}

void MainWindow::setWindowIconFromAssets()
{
    const QString pngPath = "Assets/zipline Logo.png";
    if (QFile::exists(pngPath)) {
        QPixmap pixmap(pngPath);
        if (!pixmap.isNull()) {
            QIcon icon;
            icon.addPixmap(pixmap);
            for (int size : {16, 20, 24, 32, 40, 48, 64})
                icon.addPixmap(pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            setWindowIcon(icon);
            if (auto *app = qApp) app->setWindowIcon(icon);
        }
    }
}

void MainWindow::setupUi()
{
    auto *central = new QWidget();
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    menuBar_ = new MainMenuBar(this);
    setMenuBar(menuBar_);

    toolbar_ = new MainToolBar(this);
    addToolBar(Qt::TopToolBarArea, toolbar_);

    auto *splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter);

    fileList_ = new FileListWidget(this);
    splitter->addWidget(fileList_);

    settingsPanel_ = new ArchiveSettingsWidget(this);
    splitter->addWidget(settingsPanel_);

    splitter->setSizes({700, 300});

    setupStatusBar();
}

void MainWindow::setupStatusBar()
{
    statusBar_ = new QStatusBar();
    setStatusBar(statusBar_);

    statusLabel_ = new QLabel("Ready");
    statusBar_->addWidget(statusLabel_);

    progressBar_ = new QProgressBar();
    progressBar_->setVisible(false);
    progressBar_->setMaximumWidth(200);
    statusBar_->addPermanentWidget(progressBar_);

    fileCountLabel_ = new QLabel("0 files");
    statusBar_->addPermanentWidget(fileCountLabel_);
}

void MainWindow::setupConnections()
{
    connect(menuBar_->newArchive, &QAction::triggered, this, &MainWindow::newArchive);
    connect(menuBar_->openArchive, &QAction::triggered, this, &MainWindow::openArchive);
    connect(menuBar_->addFiles, &QAction::triggered, this, &MainWindow::addFiles);
    connect(menuBar_->addFolder, &QAction::triggered, this, &MainWindow::addFolder);
    connect(menuBar_->extractAll, &QAction::triggered, this, &MainWindow::extractAll);
    connect(menuBar_->testArchive, &QAction::triggered, this, &MainWindow::testCurrentArchive);
    connect(menuBar_->aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    connect(toolbar_, &MainToolBar::newClicked, this, &MainWindow::newArchive);
    connect(toolbar_, &MainToolBar::openClicked, this, &MainWindow::openArchive);
    connect(toolbar_, &MainToolBar::addClicked, this, &MainWindow::addFilesOrFolders);
    connect(toolbar_, &MainToolBar::extractClicked, this, &MainWindow::extractAll);
    connect(toolbar_, &MainToolBar::settingsClicked, this, &MainWindow::showSettings);

    connect(fileList_, &FileListWidget::filesChanged, this, &MainWindow::updateFileCount);

    connect(settingsPanel_, &ArchiveSettingsWidget::createArchiveRequested,
            this, &MainWindow::createArchive);
}

void MainWindow::applyAppleStyle()
{
    setStyleSheet(
        "QMainWindow { background-color: #f5f5f7; color: #1d1d1f; }"
        "QToolBar { background-color: #ffffff; border: none; border-bottom: 1px solid #d2d2d7; spacing: 8px; padding: 4px; }"
        "QToolBar QToolButton { background-color: transparent; border: none; border-radius: 6px; padding: 8px; margin: 2px; }"
        "QToolBar QToolButton:hover { background-color: #f0f0f0; }"
        "QToolBar QToolButton:pressed { background-color: #e0e0e0; }"
        "QStatusBar { background-color: #ffffff; border: none; border-top: 1px solid #d2d2d7; color: #86868b; }"
        "QSplitter::handle { background-color: #d2d2d7; width: 1px; }"
        "QMenuBar { background-color: #ffffff; border: none; border-bottom: 1px solid #d2d2d7; color: #1d1d1f; }"
        "QMenuBar::item { background-color: transparent; padding: 6px 12px; margin: 0px; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #007aff; color: white; }"
        "QMenu { background-color: #ffffff; border: 1px solid #d2d2d7; border-radius: 8px; padding: 4px; color: #1d1d1f; }"
        "QMenu::item { padding: 6px 12px; border-radius: 4px; margin: 1px; }"
        "QMenu::item:selected { background-color: #007aff; color: white; }"
        "QProgressBar { border: none; border-radius: 4px; background-color: #e5e5ea; text-align: center; font-size: 11px; color: #1d1d1f; }"
        "QProgressBar::chunk { background-color: #007aff; border-radius: 4px; }");
}

void MainWindow::newArchive()
{
    fileList_->clearFiles();
    currentArchivePath_.clear();
    setWindowTitle("Zipline Archive Manager - New Archive");
    statusLabel_->setText("New archive created. Add files to begin.");
}

void MainWindow::openArchive()
{
    QString path = QFileDialog::getOpenFileName(this, "Open Archive", "",
        "Archive files (*.zip *.rar *.7z *.tar *.gz *.bz2 *.xz *.exe);;All files (*.*)");
    if (!path.isEmpty())
        loadArchive(path);
}

void MainWindow::loadArchive(const QString &path)
{
    try {
        progressBar_->setVisible(true);
        progressBar_->setRange(0, 0);
        auto entries = manager_.listContents(path.toStdString());
        fileList_->loadArchiveFiles(entries);
        currentArchivePath_ = path;
        setWindowTitle("Zipline Archive Manager - " + QFileInfo(path).fileName());
        statusLabel_->setText("Loaded archive: " + QFileInfo(path).fileName());
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Error", QString("Failed to open archive: ") + e.what());
    }
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
}

void MainWindow::addFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Add", "", "All files (*.*)");
    if (!files.isEmpty()) {
        fileList_->addFiles(files);
        statusLabel_->setText(QString("Added %1 file(s)").arg(files.size()));
    }
}

void MainWindow::addFolder()
{
    QString folder = QFileDialog::getExistingDirectory(this, "Select Folder to Add", "",
                                                       QFileDialog::ShowDirsOnly);
    if (!folder.isEmpty()) {
        fileList_->addFiles({folder});
        statusLabel_->setText("Added folder: " + QFileInfo(folder).fileName());
    }
}

void MainWindow::addFilesOrFolders()
{
    QMenu menu(this);
    QAction *addFilesAction = menu.addAction("\U0001F4C4 Add Files...");
    connect(addFilesAction, &QAction::triggered, this, &MainWindow::addFiles);
    QAction *addFolderAction = menu.addAction("\U0001F4C1 Add Folder...");
    connect(addFolderAction, &QAction::triggered, this, &MainWindow::addFolder);
    menu.exec(mapToGlobal(toolbar_->geometry().center()));
}

void MainWindow::extractAll()
{
    if (currentArchivePath_.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No archive is currently open.");
        return;
    }
    QString path = QFileDialog::getExistingDirectory(this, "Extract To");
    if (!path.isEmpty())
        extractArchiveTo(path);
}

void MainWindow::extractArchiveTo(const QString &extractPath)
{
    try {
        progressBar_->setVisible(true);
        progressBar_->setRange(0, 0);
        manager_.extractArchive(currentArchivePath_.toStdString(), extractPath.toStdString());
        statusLabel_->setText("Extracted to: " + extractPath);
        QMessageBox::information(this, "Success", "Archive extracted successfully!");
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Error", QString("Failed to extract archive: ") + e.what());
    }
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
}

void MainWindow::testCurrentArchive()
{
    if (currentArchivePath_.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No archive is currently open.");
        return;
    }
    try {
        manager_.testArchive(currentArchivePath_.toStdString());
        QMessageBox::information(this, "Test Archive", "Archive integrity verified successfully!");
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Test Archive", QString("Archive test failed: ") + e.what());
    }
}

void MainWindow::createArchive(const zipline::ArchiveSettings &settings)
{
    if (fileList_->files().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please add files before creating an archive.");
        return;
    }

    if (settings.format == "EXE") {
        auto reply = QMessageBox::question(this, "EXE Creation Notice",
            "Creating a self-extracting EXE embeds the files into a native Windows "
            "installer stub.\n\nContinue?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::No)
            return;
    }

    QMap<QString, QString> filterMap = {
        {"ZIP", "ZIP files (*.zip)"}, {"EXE", "Executable files (*.exe)"},
        {"RAR", "RAR files (*.rar)"}, {"7Z", "7Z files (*.7z)"},
        {"TAR", "TAR files (*.tar)"}, {"GZIP", "GZIP files (*.tar.gz)"}};
    QString fmt = QString::fromStdString(settings.format);

    QString savePath = QFileDialog::getSaveFileName(this, "Save Archive", "",
        filterMap.value(fmt, "All files (*.*)"));
    if (savePath.isEmpty())
        return;

    /* ensure extension */
    QString ext = "." + fmt.toLower();
    if (fmt == "GZIP") ext = ".tar.gz";
    if (!savePath.endsWith(ext, Qt::CaseInsensitive) && fmt != "RAR")
        savePath += ext;

    progressBar_->setVisible(true);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    statusLabel_->setText("Creating archive...");

    worker_ = std::make_unique<ArchiveCreationWorker>(
        &manager_, fileList_->files(), savePath, settings);
    connect(worker_.get(), &ArchiveCreationWorker::progressUpdate,
            this, &MainWindow::onArchiveProgress);
    connect(worker_.get(), &ArchiveCreationWorker::finishedSignal,
            this, &MainWindow::onArchiveFinished);
    worker_->start();
}

void MainWindow::onArchiveProgress(int percentage)
{
    progressBar_->setValue(percentage);
}

void MainWindow::onArchiveFinished(bool success, const QString &message)
{
    progressBar_->setVisible(false);
    progressBar_->setValue(0);
    if (success) {
        statusLabel_->setText(message);
        QMessageBox::information(this, "Success", "Archive created successfully!");
    } else {
        statusLabel_->setText("Archive creation failed");
        QMessageBox::critical(this, "Error", message);
    }
}

void MainWindow::showSettings()
{
    settingsPanel_->setVisible(!settingsPanel_->isVisible());
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Zipline",
        "Zipline Archive Manager v1.0.0\n\n"
        "A professional archive creation and extraction tool.\n"
        "Similar to WinRAR, 7-Zip, and Bandizip.\n\n"
        "Supports ZIP, RAR, 7Z, TAR, GZIP and self-extracting EXE formats.");
}

void MainWindow::updateFileCount(int count)
{
    fileCountLabel_->setText(QString("%1 file%2").arg(count).arg(count != 1 ? "s" : ""));
    /* expose the current file list to the settings panel (EXE dialog needs it) */
    setProperty("currentFiles", fileList_->files());
}
