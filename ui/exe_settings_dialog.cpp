#include "exe_settings_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

ExeSettingsDialog::ExeSettingsDialog(QWidget *parent, const QStringList &fileList)
    : QDialog(parent), fileList_(fileList)
{
    setWindowTitle("Advanced EXE Settings");
    setModal(true);
    setMinimumSize(700, 600);
    resize(800, 700);
    setupUi();
    applyStyles();
}

void ExeSettingsDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    tabs_ = new QTabWidget();
    layout->addWidget(tabs_);

    setupAppearanceTab();
    setupExtractionTab();
    setupShortcutsTab();
    setupAdvancedTab();

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    auto *okButton = new QPushButton("OK");
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(okButton);

    auto *cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    layout->addLayout(buttonLayout);
}

void ExeSettingsDialog::setupAppearanceTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);

    auto *iconGroup = new QGroupBox("Executable Icon");
    auto *iconLayout = new QGridLayout(iconGroup);

    iconPreview_ = new QLabel();
    iconPreview_->setFixedSize(64, 64);
    iconPreview_->setStyleSheet("border: 1px solid #d2d2d7; background: white;");
    iconPreview_->setAlignment(Qt::AlignCenter);
    iconPreview_->setText("No Icon");
    iconLayout->addWidget(new QLabel("Preview:"), 0, 0);
    iconLayout->addWidget(iconPreview_, 0, 1);

    iconLayout->addWidget(new QLabel("Icon File:"), 1, 0);
    iconPath_ = new QLineEdit();
    iconPath_->setPlaceholderText("Select .ico file for executable");
    iconLayout->addWidget(iconPath_, 1, 1);

    auto *browseIconBtn = new QPushButton("Browse...");
    connect(browseIconBtn, &QPushButton::clicked, this, &ExeSettingsDialog::browseIcon);
    iconLayout->addWidget(browseIconBtn, 1, 2);

    useZiplineIcon_ = new QCheckBox("Use Zipline default icon");
    useZiplineIcon_->setChecked(true);
    connect(useZiplineIcon_, &QCheckBox::toggled, this, &ExeSettingsDialog::onUseDefaultIcon);
    iconLayout->addWidget(useZiplineIcon_, 2, 1);

    layout->addWidget(iconGroup);

    auto *titleGroup = new QGroupBox("Window Appearance");
    auto *titleLayout = new QGridLayout(titleGroup);

    titleLayout->addWidget(new QLabel("Window Title:"), 0, 0);
    windowTitle_ = new QLineEdit("Zipline Self-Extracting Archive");
    titleLayout->addWidget(windowTitle_, 0, 1);

    titleLayout->addWidget(new QLabel("Description:"), 1, 0);
    description_ = new QTextEdit();
    description_->setMaximumHeight(80);
    description_->setText("This archive will extract files to a folder you choose.\n"
                          "Click 'Extract' to select destination and begin extraction.");
    titleLayout->addWidget(description_, 1, 1);

    layout->addWidget(titleGroup);
    layout->addStretch();

    tabs_->addTab(tab, "Appearance");
}

void ExeSettingsDialog::setupExtractionTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);

    auto *extractGroup = new QGroupBox("Default Extraction Settings");
    auto *extractLayout = new QGridLayout(extractGroup);

    extractLayout->addWidget(new QLabel("Default Extract To:"), 0, 0);
    defaultPath_ = new QComboBox();
    defaultPath_->setEditable(true);
    defaultPath_->addItems({
        "C:\\Program Files\\Rivelsoft",
        "%USERPROFILE%\\Desktop",
        "%USERPROFILE%\\Documents",
        "%USERPROFILE%\\Downloads",
        "%TEMP%",
        "C:\\Program Files",
        "C:\\Program Files (x86)",
        "Same folder as executable",
        "Ask user (show browse dialog)"});
    extractLayout->addWidget(defaultPath_, 0, 1);

    autoExtract_ = new QCheckBox("Auto-extract without showing dialog");
    extractLayout->addWidget(autoExtract_, 1, 0, 1, 2);

    createSubfolder_ = new QCheckBox("Create subfolder with archive name");
    createSubfolder_->setChecked(true);
    extractLayout->addWidget(createSubfolder_, 2, 0, 1, 2);

    overwriteFiles_ = new QCheckBox("Overwrite existing files without asking");
    extractLayout->addWidget(overwriteFiles_, 3, 0, 1, 2);

    layout->addWidget(extractGroup);

    auto *postGroup = new QGroupBox("After Extraction");
    auto *postLayout = new QVBoxLayout(postGroup);

    openFolder_ = new QCheckBox("Open extracted folder in Windows Explorer");
    openFolder_->setChecked(true);
    postLayout->addWidget(openFolder_);

    runFile_ = new QCheckBox("Run a file after extraction:");
    postLayout->addWidget(runFile_);

    auto *runLayout = new QHBoxLayout();
    runFilePath_ = new QLineEdit();
    runFilePath_->setPlaceholderText("e.g., setup.exe, readme.txt");
    runFilePath_->setEnabled(false);
    runLayout->addWidget(runFilePath_);

    auto *browseRunFile = new QPushButton("Browse...");
    browseRunFile->setEnabled(false);
    connect(browseRunFile, &QPushButton::clicked, this, &ExeSettingsDialog::browseRunFile);
    runLayout->addWidget(browseRunFile);
    postLayout->addLayout(runLayout);

    connect(runFile_, &QCheckBox::toggled, runFilePath_, &QWidget::setEnabled);
    connect(runFile_, &QCheckBox::toggled, browseRunFile, &QWidget::setEnabled);

    deleteAfterRun_ = new QCheckBox("Delete extracted files after running");
    deleteAfterRun_->setEnabled(false);
    connect(runFile_, &QCheckBox::toggled, deleteAfterRun_, &QWidget::setEnabled);
    postLayout->addWidget(deleteAfterRun_);

    layout->addWidget(postGroup);
    layout->addStretch();

    tabs_->addTab(tab, "Extraction");
}

void ExeSettingsDialog::setupShortcutsTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);

    auto *desktopGroup = new QGroupBox("Desktop Shortcuts");
    auto *desktopLayout = new QVBoxLayout(desktopGroup);

    createDesktopShortcut_ = new QCheckBox("Create desktop shortcut after extraction");
    desktopLayout->addWidget(createDesktopShortcut_);

    auto *shortcutLayout = new QGridLayout();
    shortcutLayout->addWidget(new QLabel("Shortcut Name:"), 0, 0);
    desktopShortcutName_ = new QLineEdit("My Application");
    desktopShortcutName_->setEnabled(false);
    shortcutLayout->addWidget(desktopShortcutName_, 0, 1, 1, 2);

    shortcutLayout->addWidget(new QLabel("Target File:"), 1, 0);
    desktopTargetFile_ = new QLineEdit();
    desktopTargetFile_->setPlaceholderText("e.g., program.exe");
    desktopTargetFile_->setEnabled(false);
    shortcutLayout->addWidget(desktopTargetFile_, 1, 1);

    auto *browseTargetButton = new QPushButton("Browse...");
    browseTargetButton->setEnabled(false);
    connect(browseTargetButton, &QPushButton::clicked, this, &ExeSettingsDialog::browseArchiveFiles);
    shortcutLayout->addWidget(browseTargetButton, 1, 2);

    connect(createDesktopShortcut_, &QCheckBox::toggled, desktopShortcutName_, &QWidget::setEnabled);
    connect(createDesktopShortcut_, &QCheckBox::toggled, desktopTargetFile_, &QWidget::setEnabled);
    connect(createDesktopShortcut_, &QCheckBox::toggled, browseTargetButton, &QWidget::setEnabled);

    desktopLayout->addLayout(shortcutLayout);
    layout->addWidget(desktopGroup);

    auto *startmenuGroup = new QGroupBox("Start Menu Integration");
    auto *startmenuLayout = new QVBoxLayout(startmenuGroup);

    createStartmenuShortcut_ = new QCheckBox("Create Start Menu shortcut");
    startmenuLayout->addWidget(createStartmenuShortcut_);

    auto *startmenuDetails = new QGridLayout();
    startmenuDetails->addWidget(new QLabel("Program Group:"), 0, 0);
    startmenuGroup_ = new QLineEdit("My Software");
    startmenuGroup_->setEnabled(false);
    startmenuDetails->addWidget(startmenuGroup_, 0, 1);

    startmenuDetails->addWidget(new QLabel("Shortcut Name:"), 1, 0);
    startmenuShortcutName_ = new QLineEdit("My Application");
    startmenuShortcutName_->setEnabled(false);
    startmenuDetails->addWidget(startmenuShortcutName_, 1, 1);

    connect(createStartmenuShortcut_, &QCheckBox::toggled, startmenuGroup_, &QWidget::setEnabled);
    connect(createStartmenuShortcut_, &QCheckBox::toggled, startmenuShortcutName_, &QWidget::setEnabled);

    startmenuLayout->addLayout(startmenuDetails);
    layout->addWidget(startmenuGroup);

    layout->addStretch();
    tabs_->addTab(tab, "Shortcuts");
}

void ExeSettingsDialog::setupAdvancedTab()
{
    auto *tab = new QWidget();
    auto *layout = new QVBoxLayout(tab);

    auto *envGroup = new QGroupBox("System Integration");
    auto *envLayout = new QVBoxLayout(envGroup);

    addToPath_ = new QCheckBox("Add extraction folder to Windows PATH environment variable");
    envLayout->addWidget(addToPath_);

    auto *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel("Add to PATH:"));
    pathSubfolder_ = new QLineEdit();
    pathSubfolder_->setPlaceholderText("Leave empty to add root folder, or specify subfolder like 'bin'");
    pathSubfolder_->setEnabled(false);
    pathLayout->addWidget(pathSubfolder_);
    connect(addToPath_, &QCheckBox::toggled, pathSubfolder_, &QWidget::setEnabled);
    envLayout->addLayout(pathLayout);

    createRegistryEntries_ = new QCheckBox("Create Windows Registry entries for uninstaller");
    envLayout->addWidget(createRegistryEntries_);

    auto *regLayout = new QGridLayout();
    regLayout->addWidget(new QLabel("Program Name:"), 0, 0);
    registryProgramName_ = new QLineEdit("My Application");
    registryProgramName_->setEnabled(false);
    regLayout->addWidget(registryProgramName_, 0, 1);

    regLayout->addWidget(new QLabel("Version:"), 1, 0);
    registryVersion_ = new QLineEdit("1.0.0");
    registryVersion_->setEnabled(false);
    regLayout->addWidget(registryVersion_, 1, 1);

    regLayout->addWidget(new QLabel("Publisher:"), 2, 0);
    registryPublisher_ = new QLineEdit("My Company");
    registryPublisher_->setEnabled(false);
    regLayout->addWidget(registryPublisher_, 2, 1);

    connect(createRegistryEntries_, &QCheckBox::toggled, registryProgramName_, &QWidget::setEnabled);
    connect(createRegistryEntries_, &QCheckBox::toggled, registryVersion_, &QWidget::setEnabled);
    connect(createRegistryEntries_, &QCheckBox::toggled, registryPublisher_, &QWidget::setEnabled);

    envLayout->addLayout(regLayout);
    layout->addWidget(envGroup);

    auto *securityGroup = new QGroupBox("Security & Permissions");
    auto *securityLayout = new QVBoxLayout(securityGroup);

    requireAdmin_ = new QCheckBox("Require administrator privileges for extraction");
    securityLayout->addWidget(requireAdmin_);

    silentMode_ = new QCheckBox("Support silent extraction mode (/S parameter)");
    securityLayout->addWidget(silentMode_);

    verifySignature_ = new QCheckBox("Verify digital signature before extraction");
    securityLayout->addWidget(verifySignature_);

    layout->addWidget(securityGroup);
    layout->addStretch();

    tabs_->addTab(tab, "Advanced");
}

void ExeSettingsDialog::applyStyles()
{
    setStyleSheet(
        "QDialog { background-color: #f5f5f7; color: #1d1d1f; }"
        "QTabWidget::pane { border: 1px solid #d2d2d7; background-color: #ffffff; border-radius: 8px; }"
        "QTabBar::tab { background-color: #f0f0f0; color: #1d1d1f; padding: 8px 16px;"
        "    margin-right: 2px; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background-color: #ffffff; color: #007aff; font-weight: bold; }"
        "QGroupBox { font-weight: bold; border: 1px solid #d2d2d7; border-radius: 8px;"
        "    margin-top: 8px; padding-top: 8px; background-color: #ffffff; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px 0 4px;"
        "    background-color: #ffffff; color: #1d1d1f; }"
        "QPushButton { background-color: #007aff; color: white; border: none;"
        "    border-radius: 6px; padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background-color: #0056cc; }"
        "QPushButton:pressed { background-color: #004499; }"
        "QLineEdit, QComboBox, QTextEdit { border: 1px solid #d2d2d7; border-radius: 6px;"
        "    padding: 6px 8px; background-color: #ffffff; color: #1d1d1f; }"
        "QLineEdit:focus, QComboBox:focus, QTextEdit:focus { border-color: #007aff; }"
        "QCheckBox { spacing: 8px; color: #1d1d1f; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #d2d2d7;"
        "    border-radius: 3px; background-color: #ffffff; }"
        "QCheckBox::indicator:checked { background-color: #007aff; border-color: #007aff; }");
}

void ExeSettingsDialog::browseIcon()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Icon File", "",
                                                "Icon files (*.ico);;All files (*.*)");
    if (!path.isEmpty()) {
        iconPath_->setText(path);
        useZiplineIcon_->setChecked(false);
        loadIconPreview(path);
    }
}

void ExeSettingsDialog::loadIconPreview(const QString &path)
{
    QPixmap pixmap(path);
    if (!pixmap.isNull()) {
        iconPreview_->setPixmap(pixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconPreview_->setText("");
    } else {
        iconPreview_->setText("Invalid Icon");
    }
}

void ExeSettingsDialog::onUseDefaultIcon(bool checked)
{
    if (checked) {
        iconPath_->clear();
        iconPreview_->setText("Zipline Default");
    } else {
        iconPreview_->setText("No Icon");
    }
}

void ExeSettingsDialog::browseArchiveFiles()
{
    if (fileList_.isEmpty()) {
        QMessageBox::information(this, "No Files",
            "No files have been added to the archive yet.\n\n"
            "Please add files to archive first, then configure EXE settings.");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Select Target File from Archive");
    dialog.setMinimumSize(500, 400);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Select a file from the archive to create a desktop shortcut:"));

    auto *listWidget = new QListWidget();
    for (const QString &filePath : fileList_) {
        QFileInfo fi(filePath);
        if (fi.isFile()) {
            listWidget->addItem(fi.fileName());
        } else {
            QDirIterator it(filePath, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString full = it.next();
                listWidget->addItem(QDir(fi.absolutePath()).relativeFilePath(full));
            }
        }
    }
    connect(listWidget, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    layout->addWidget(listWidget);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        auto items = listWidget->selectedItems();
        if (!items.isEmpty())
            desktopTargetFile_->setText(items.first()->text());
    }
}

void ExeSettingsDialog::browseRunFile()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Select File to Run After Extraction", "",
        "Executable files (*.exe *.bat *.cmd);;All files (*.*)");
    if (!path.isEmpty())
        runFilePath_->setText(QFileInfo(path).fileName());
}

static std::string b(bool v) { return v ? "1" : "0"; }

std::map<std::string, std::string> ExeSettingsDialog::getSettings() const
{
    std::map<std::string, std::string> s;
    s["custom_icon"] = useZiplineIcon_->isChecked() ? "" : iconPath_->text().toStdString();
    s["use_zipline_icon"] = b(useZiplineIcon_->isChecked());
    s["window_title"] = windowTitle_->text().toStdString();
    s["description"] = description_->toPlainText().toStdString();

    s["default_extract_path"] = defaultPath_->currentText().toStdString();
    s["auto_extract"] = b(autoExtract_->isChecked());
    s["create_subfolder"] = b(createSubfolder_->isChecked());
    s["overwrite_files"] = b(overwriteFiles_->isChecked());
    s["open_folder"] = b(openFolder_->isChecked());
    s["run_file"] = b(runFile_->isChecked());
    s["run_file_path"] = runFilePath_->text().toStdString();
    s["delete_after_run"] = b(deleteAfterRun_->isChecked());

    s["create_desktop_shortcut"] = b(createDesktopShortcut_->isChecked());
    s["desktop_shortcut_name"] = desktopShortcutName_->text().toStdString();
    s["desktop_target_file"] = desktopTargetFile_->text().toStdString();
    s["create_startmenu_shortcut"] = b(createStartmenuShortcut_->isChecked());
    s["startmenu_group"] = startmenuGroup_->text().toStdString();
    s["startmenu_shortcut_name"] = startmenuShortcutName_->text().toStdString();

    s["add_to_path"] = b(addToPath_->isChecked());
    s["path_subfolder"] = pathSubfolder_->text().toStdString();
    s["create_registry_entries"] = b(createRegistryEntries_->isChecked());
    s["registry_program_name"] = registryProgramName_->text().toStdString();
    s["registry_version"] = registryVersion_->text().toStdString();
    s["registry_publisher"] = registryPublisher_->text().toStdString();
    s["require_admin"] = b(requireAdmin_->isChecked());
    s["silent_mode"] = b(silentMode_->isChecked());
    s["verify_signature"] = b(verifySignature_->isChecked());
    return s;
}
