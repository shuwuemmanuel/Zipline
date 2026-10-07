/* Advanced EXE Settings Dialog (C++ port of ui/exe_settings_dialog.py) */
#ifndef ZIPLINE_EXE_SETTINGS_DIALOG_H
#define ZIPLINE_EXE_SETTINGS_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QTextEdit>
#include <QLabel>
#include <QTabWidget>
#include <QStringList>
#include <map>
#include <string>

class ExeSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExeSettingsDialog(QWidget *parent = nullptr,
                               const QStringList &fileList = {});

    std::map<std::string, std::string> getSettings() const;

private slots:
    void browseIcon();
    void onUseDefaultIcon(bool checked);
    void browseArchiveFiles();
    void browseRunFile();

private:
    void setupUi();
    void setupAppearanceTab();
    void setupExtractionTab();
    void setupShortcutsTab();
    void setupAdvancedTab();
    void applyStyles();
    void loadIconPreview(const QString &path);

    QStringList fileList_;
    QTabWidget *tabs_ = nullptr;

    // Appearance
    QLabel *iconPreview_ = nullptr;
    QLineEdit *iconPath_ = nullptr;
    QCheckBox *useZiplineIcon_ = nullptr;
    QLineEdit *windowTitle_ = nullptr;
    QTextEdit *description_ = nullptr;

    // Extraction
    QComboBox *defaultPath_ = nullptr;
    QCheckBox *autoExtract_ = nullptr;
    QCheckBox *createSubfolder_ = nullptr;
    QCheckBox *overwriteFiles_ = nullptr;
    QCheckBox *openFolder_ = nullptr;
    QCheckBox *runFile_ = nullptr;
    QLineEdit *runFilePath_ = nullptr;
    QCheckBox *deleteAfterRun_ = nullptr;

    // Shortcuts
    QCheckBox *createDesktopShortcut_ = nullptr;
    QLineEdit *desktopShortcutName_ = nullptr;
    QLineEdit *desktopTargetFile_ = nullptr;
    QCheckBox *createStartmenuShortcut_ = nullptr;
    QLineEdit *startmenuGroup_ = nullptr;
    QLineEdit *startmenuShortcutName_ = nullptr;

    // Advanced
    QCheckBox *addToPath_ = nullptr;
    QLineEdit *pathSubfolder_ = nullptr;
    QCheckBox *createRegistryEntries_ = nullptr;
    QLineEdit *registryProgramName_ = nullptr;
    QLineEdit *registryVersion_ = nullptr;
    QLineEdit *registryPublisher_ = nullptr;
    QCheckBox *requireAdmin_ = nullptr;
    QCheckBox *silentMode_ = nullptr;
    QCheckBox *verifySignature_ = nullptr;
};

#endif
