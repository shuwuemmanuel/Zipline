/* Archive Settings Widget (C++ port of ui/archive_settings.py) */
#ifndef ZIPLINE_ARCHIVE_SETTINGS_H
#define ZIPLINE_ARCHIVE_SETTINGS_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <map>
#include <string>

#include "collapsible_widget.h"
#include "../core/archive_manager.h"

class ArchiveSettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ArchiveSettingsWidget(QWidget *parent = nullptr);

    zipline::ArchiveSettings getSettings() const;

signals:
    void createArchiveRequested(const zipline::ArchiveSettings &settings);

private slots:
    void onFormatChanged(const QString &text);
    void updateCompressionLabel(int value);
    void applyPreset(const QString &preset);
    void createArchiveClicked();
    void showExeAdvancedSettings();

private:
    void setupUi();
    QLayout *createFormatSection();
    QLayout *createCompressionSection();
    QLayout *createAdvancedSection();
    QLayout *createPasswordSection();
    QLayout *createOutputSection();
    void setupActionButtons(QVBoxLayout *layout);
    void applyStyles();

    CollapsibleGroup *group_ = nullptr;

    QComboBox *formatCombo_ = nullptr;
    QLabel    *formatInfo_ = nullptr;
    QSlider   *compressionSlider_ = nullptr;
    QLabel    *compressionLabel_ = nullptr;
    QComboBox *methodCombo_ = nullptr;
    QComboBox *dictCombo_ = nullptr;
    QCheckBox *splitCheck_ = nullptr;
    QSpinBox  *volumeSize_ = nullptr;
    QCheckBox *solidCheck_ = nullptr;
    QCheckBox *deleteCheck_ = nullptr;
    QCheckBox *attributesCheck_ = nullptr;
    QCheckBox *symlinksCheck_ = nullptr;
    QCheckBox *passwordCheck_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QLineEdit *passwordConfirm_ = nullptr;
    QComboBox *encryptionCombo_ = nullptr;
    QCheckBox *encryptNamesCheck_ = nullptr;
    QPushButton *exeAdvancedButton_ = nullptr;
    QTextEdit *commentEdit_ = nullptr;
    QCheckBox *testCheck_ = nullptr;
    QCheckBox *progressCheck_ = nullptr;
    QComboBox *presetsCombo_ = nullptr;
    QPushButton *createButton_ = nullptr;

    std::map<std::string, std::string> exeAdvancedSettings_;
};

#endif
