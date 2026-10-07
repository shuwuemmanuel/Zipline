#include "archive_settings.h"
#include "exe_settings_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFont>

ArchiveSettingsWidget::ArchiveSettingsWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void ArchiveSettingsWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *scrollContent = new QWidget();
    auto *contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->setSpacing(0);

    auto *titleLabel = new QLabel("Archive Settings");
    titleLabel->setFont(QFont("Segoe UI", 12, QFont::Bold));
    titleLabel->setStyleSheet("color: #1d1d1f; margin-bottom: 8px; padding: 4px 0px;");
    contentLayout->addWidget(titleLabel);

    group_ = new CollapsibleGroup();
    contentLayout->addWidget(group_);

    group_->addBox("\U0001F4E6 Archive Format", createFormatSection());
    group_->addBox("\U0001F5DC️ Compression Settings", createCompressionSection());
    group_->addBox("⚙️ Advanced Options", createAdvancedSection());
    group_->addBox("\U0001F510 Password Protection", createPasswordSection());
    group_->addBox("\U0001F4E4 Output Settings", createOutputSection());

    contentLayout->addStretch();
    setupActionButtons(contentLayout);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    group_->expandFirst();
    applyStyles();
}

QLayout *ArchiveSettingsWidget::createFormatSection()
{
    auto *layout = new QVBoxLayout();
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    formatCombo_ = new QComboBox();
    formatCombo_->addItems({
        "ZIP - Universal compatibility",
        "7Z - Best compression",
        "EXE - Self-extracting executable",
        "RAR - WinRAR format",
        "TAR - Unix archive",
        "GZIP - Compressed TAR"});
    formatCombo_->setCurrentText("ZIP - Universal compatibility");
    connect(formatCombo_, &QComboBox::currentTextChanged, this,
            &ArchiveSettingsWidget::onFormatChanged);

    layout->addWidget(new QLabel("Format:"));
    layout->addWidget(formatCombo_);

    formatInfo_ = new QLabel("Standard ZIP format with good compression and compatibility");
    formatInfo_->setStyleSheet("color: #86868b; font-size: 10px;");
    formatInfo_->setWordWrap(true);
    layout->addWidget(formatInfo_);

    return layout;
}

QLayout *ArchiveSettingsWidget::createCompressionSection()
{
    auto *layout = new QVBoxLayout();
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *levelLayout = new QHBoxLayout();
    levelLayout->addWidget(new QLabel("Level:"));

    compressionSlider_ = new QSlider(Qt::Horizontal);
    compressionSlider_->setMinimum(0);
    compressionSlider_->setMaximum(9);
    compressionSlider_->setValue(4);
    connect(compressionSlider_, &QSlider::valueChanged, this,
            &ArchiveSettingsWidget::updateCompressionLabel);

    compressionLabel_ = new QLabel("6 (Normal)");
    compressionLabel_->setMinimumWidth(80);

    levelLayout->addWidget(compressionSlider_);
    levelLayout->addWidget(compressionLabel_);
    layout->addLayout(levelLayout);

    auto *methodLayout = new QHBoxLayout();
    methodLayout->addWidget(new QLabel("Method:"));
    methodCombo_ = new QComboBox();
    methodCombo_->addItems({"Deflate", "Store", "BZIP2", "LZMA"});
    methodCombo_->setCurrentText("Deflate");
    methodLayout->addWidget(methodCombo_);
    layout->addLayout(methodLayout);

    auto *dictLayout = new QHBoxLayout();
    dictLayout->addWidget(new QLabel("Dictionary:"));
    dictCombo_ = new QComboBox();
    dictCombo_->addItems({"1 MB", "2 MB", "4 MB", "8 MB", "16 MB", "32 MB"});
    dictCombo_->setCurrentText("4 MB");
    dictLayout->addWidget(dictCombo_);
    layout->addLayout(dictLayout);

    return layout;
}

QLayout *ArchiveSettingsWidget::createAdvancedSection()
{
    auto *layout = new QVBoxLayout();
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    splitCheck_ = new QCheckBox("Split archive into volumes");
    layout->addWidget(splitCheck_);

    auto *splitLayout = new QHBoxLayout();
    splitLayout->addWidget(new QLabel("Volume size:"));
    volumeSize_ = new QSpinBox();
    volumeSize_->setMinimum(1);
    volumeSize_->setMaximum(999999);
    volumeSize_->setValue(100);
    volumeSize_->setSuffix(" MB");
    volumeSize_->setEnabled(false);
    connect(splitCheck_, &QCheckBox::toggled, volumeSize_, &QWidget::setEnabled);
    splitLayout->addWidget(volumeSize_);
    splitLayout->addStretch();
    layout->addLayout(splitLayout);

    solidCheck_ = new QCheckBox("Create solid archive (better compression)");
    solidCheck_->setChecked(true);
    layout->addWidget(solidCheck_);

    deleteCheck_ = new QCheckBox("Delete files after successful archiving");
    layout->addWidget(deleteCheck_);

    attributesCheck_ = new QCheckBox("Store file attributes and timestamps");
    attributesCheck_->setChecked(true);
    layout->addWidget(attributesCheck_);

    symlinksCheck_ = new QCheckBox("Store symbolic links");
    symlinksCheck_->setChecked(true);
    layout->addWidget(symlinksCheck_);

    return layout;
}

QLayout *ArchiveSettingsWidget::createPasswordSection()
{
    auto *layout = new QVBoxLayout();
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    passwordCheck_ = new QCheckBox("Encrypt archive with password");
    layout->addWidget(passwordCheck_);

    auto *passwordWidget = new QWidget();
    auto *passwordLayout = new QVBoxLayout(passwordWidget);
    passwordLayout->setContentsMargins(12, 0, 0, 0);

    auto *passLayout1 = new QHBoxLayout();
    passLayout1->addWidget(new QLabel("Password:"));
    passwordEdit_ = new QLineEdit();
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passLayout1->addWidget(passwordEdit_);
    passwordLayout->addLayout(passLayout1);

    auto *passLayout2 = new QHBoxLayout();
    passLayout2->addWidget(new QLabel("Confirm:"));
    passwordConfirm_ = new QLineEdit();
    passwordConfirm_->setEchoMode(QLineEdit::Password);
    passLayout2->addWidget(passwordConfirm_);
    passwordLayout->addLayout(passLayout2);

    auto *encLayout = new QHBoxLayout();
    encLayout->addWidget(new QLabel("Encryption:"));
    encryptionCombo_ = new QComboBox();
    encryptionCombo_->addItems({"AES-256", "AES-128", "ZipCrypto"});
    encLayout->addWidget(encryptionCombo_);
    passwordLayout->addLayout(encLayout);

    encryptNamesCheck_ = new QCheckBox("Encrypt file names");
    passwordLayout->addWidget(encryptNamesCheck_);

    passwordWidget->setEnabled(false);
    connect(passwordCheck_, &QCheckBox::toggled, passwordWidget, &QWidget::setEnabled);

    layout->addWidget(passwordWidget);

    exeAdvancedButton_ = new QPushButton("Advanced EXE Settings...");
    connect(exeAdvancedButton_, &QPushButton::clicked, this,
            &ArchiveSettingsWidget::showExeAdvancedSettings);
    exeAdvancedButton_->setVisible(false);
    exeAdvancedButton_->setStyleSheet(
        "QPushButton { background-color: #34c759; color: white; font-weight: bold;"
        "    padding: 8px 16px; margin-top: 8px; }"
        "QPushButton:hover { background-color: #2fb344; }");
    layout->addWidget(exeAdvancedButton_);

    return layout;
}

QLayout *ArchiveSettingsWidget::createOutputSection()
{
    auto *layout = new QVBoxLayout();
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    layout->addWidget(new QLabel("Archive comment:"));
    commentEdit_ = new QTextEdit();
    commentEdit_->setMaximumHeight(80);
    commentEdit_->setPlaceholderText("Optional comment for the archive...");
    layout->addWidget(commentEdit_);

    testCheck_ = new QCheckBox("Test archive after creation");
    testCheck_->setChecked(true);
    layout->addWidget(testCheck_);

    progressCheck_ = new QCheckBox("Show detailed progress");
    progressCheck_->setChecked(true);
    layout->addWidget(progressCheck_);

    return layout;
}

void ArchiveSettingsWidget::setupActionButtons(QVBoxLayout *layout)
{
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(0, 12, 0, 0);

    auto *presetsLayout = new QVBoxLayout();
    presetsLayout->addWidget(new QLabel("Presets:"));

    presetsCombo_ = new QComboBox();
    presetsCombo_->addItems({"Default", "Best Speed", "Best Compression",
                             "Email Friendly", "Backup Archive", "Self-Extracting", "Custom"});
    connect(presetsCombo_, &QComboBox::currentTextChanged, this,
            &ArchiveSettingsWidget::applyPreset);
    presetsLayout->addWidget(presetsCombo_);

    buttonLayout->addLayout(presetsLayout);
    buttonLayout->addStretch();

    createButton_ = new QPushButton("Create Archive");
    connect(createButton_, &QPushButton::clicked, this,
            &ArchiveSettingsWidget::createArchiveClicked);
    createButton_->setStyleSheet(
        "QPushButton { background-color: #007aff; color: white; border: none;"
        "    border-radius: 8px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #0056cc; }"
        "QPushButton:pressed { background-color: #004499; }");
    buttonLayout->addWidget(createButton_);

    layout->addLayout(buttonLayout);
}

void ArchiveSettingsWidget::applyStyles()
{
    setStyleSheet(
        "QWidget { background-color: #ffffff; color: #1d1d1f; }"
        "QComboBox, QSpinBox, QLineEdit { border: 1px solid #d2d2d7; border-radius: 6px;"
        "    padding: 8px 12px; background-color: #ffffff; color: #1d1d1f; font-size: 13px; min-height: 20px; }"
        "QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color: #007aff; }"
        "QSlider::groove:horizontal { border: 1px solid #d2d2d7; height: 4px; background: #f0f0f0; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #007aff; border: none; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }"
        "QSlider::sub-page:horizontal { background: #007aff; border-radius: 2px; }"
        "QCheckBox { spacing: 10px; font-size: 13px; padding: 4px 0px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #d2d2d7; border-radius: 3px; background-color: #ffffff; }"
        "QCheckBox::indicator:checked { background-color: #007aff; border-color: #007aff; }"
        "QTextEdit { border: 1px solid #d2d2d7; border-radius: 6px; padding: 8px; background-color: #ffffff; color: #1d1d1f; }"
        "QTextEdit:focus { border-color: #007aff; }"
        "QScrollArea { border: none; background-color: #ffffff; }");
}

void ArchiveSettingsWidget::onFormatChanged(const QString &formatText)
{
    static const std::map<QString, QString> info = {
        {"ZIP - Universal compatibility", "Standard ZIP format with good compression and compatibility"},
        {"7Z - Best compression", "Advanced 7-Zip format with excellent compression ratios"},
        {"EXE - Self-extracting executable", "Self-extracting Windows executable that runs without additional software"},
        {"RAR - WinRAR format", "WinRAR proprietary format with good compression"},
        {"TAR - Unix archive", "Unix TAR format for file archiving without compression"},
        {"GZIP - Compressed TAR", "TAR archive compressed with GZIP"}};
    auto it = info.find(formatText);
    formatInfo_->setText(it != info.end() ? it->second : "");

    QString key = formatText.split(" - ").first();
    solidCheck_->setEnabled(key == "7Z" || key == "EXE" || key == "RAR");
    dictCombo_->setEnabled(key == "7Z" || key == "EXE");
    passwordCheck_->setEnabled(key == "ZIP" || key == "7Z" || key == "EXE" || key == "RAR");
    if (exeAdvancedButton_)
        exeAdvancedButton_->setVisible(key == "EXE");
}

void ArchiveSettingsWidget::updateCompressionLabel(int value)
{
    static const char *labels[] = {
        "0 (Store)", "1 (Fastest)", "2 (Fast)", "3 (Fast)", "4 (Fast)",
        "5 (Normal)", "6 (Normal)", "7 (Maximum)", "8 (Maximum)", "9 (Ultra)"};
    if (value >= 0 && value <= 9)
        compressionLabel_->setText(labels[value]);
    else
        compressionLabel_->setText(QString::number(value));
}

void ArchiveSettingsWidget::applyPreset(const QString &presetName)
{
    if (presetName == "Best Speed") {
        compressionSlider_->setValue(1); methodCombo_->setCurrentText("Store"); solidCheck_->setChecked(false);
    } else if (presetName == "Best Compression") {
        compressionSlider_->setValue(9); methodCombo_->setCurrentText("LZMA"); solidCheck_->setChecked(true);
    } else if (presetName == "Email Friendly") {
        compressionSlider_->setValue(6); methodCombo_->setCurrentText("Deflate");
        splitCheck_->setChecked(true); volumeSize_->setValue(25);
    } else if (presetName == "Backup Archive") {
        compressionSlider_->setValue(5); methodCombo_->setCurrentText("Deflate");
        solidCheck_->setChecked(true); testCheck_->setChecked(true);
    } else if (presetName == "Self-Extracting") {
        compressionSlider_->setValue(7); methodCombo_->setCurrentText("LZMA"); solidCheck_->setChecked(true);
        int idx = formatCombo_->findText("EXE", Qt::MatchContains);
        if (idx >= 0) formatCombo_->setCurrentIndex(idx);
    }
}

void ArchiveSettingsWidget::createArchiveClicked()
{
    emit createArchiveRequested(getSettings());
}

zipline::ArchiveSettings ArchiveSettingsWidget::getSettings() const
{
    zipline::ArchiveSettings s;
    s.format = formatCombo_->currentText().split(" - ").first().toStdString();
    s.compression_level = compressionSlider_->value();
    s.compression_method = methodCombo_->currentText().toStdString();
    s.dictionary_size = dictCombo_->currentText().toStdString();
    s.solid_archive = solidCheck_->isChecked() && solidCheck_->isEnabled();
    s.split_archive = splitCheck_->isChecked();
    s.volume_size = volumeSize_->value();
    s.delete_after = deleteCheck_->isChecked();
    s.store_attributes = attributesCheck_->isChecked();
    s.store_symlinks = symlinksCheck_->isChecked();
    s.password_protected = passwordCheck_->isChecked() && passwordCheck_->isEnabled();
    s.password = passwordCheck_->isChecked() ? passwordEdit_->text().toStdString() : "";
    s.encryption_method = encryptionCombo_->currentText().toStdString();
    s.encrypt_filenames = encryptNamesCheck_->isChecked();
    s.comment = commentEdit_->toPlainText().toStdString();
    s.test_after_creation = testCheck_->isChecked();
    s.show_progress = progressCheck_->isChecked();
    if (s.format == "EXE")
        s.exe_advanced_settings = exeAdvancedSettings_;
    return s;
}

void ArchiveSettingsWidget::showExeAdvancedSettings()
{
    QStringList fileList;
    // Try to obtain the current file list from the parent (MainWindow).
    if (auto *mw = parentWidget()) {
        // MainWindow exposes files via a child FileListWidget; best effort via property.
        QVariant v = mw->property("currentFiles");
        if (v.isValid()) fileList = v.toStringList();
    }

    ExeSettingsDialog dialog(this, fileList);
    if (dialog.exec() == QDialog::Accepted) {
        exeAdvancedSettings_ = dialog.getSettings();
        int count = 0;
        for (auto &kv : exeAdvancedSettings_)
            if (!kv.second.empty() && kv.second != "0") count++;
        if (count > 0)
            exeAdvancedButton_->setText(
                QString("Advanced EXE Settings... (%1 configured)").arg(count));
        else
            exeAdvancedButton_->setText("Advanced EXE Settings...");
    }
}
