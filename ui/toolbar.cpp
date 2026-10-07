#include "toolbar.h"

#include <QLabel>
#include <QPixmap>
#include <QWidget>
#include <QSizePolicy>
#include <QFile>

MainToolBar::MainToolBar(QWidget *parent) : QToolBar(parent)
{
    setObjectName("MainToolBar");
    setMovable(false);
    setIconSize(QSize(24, 24));
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setupActions();
}

QToolButton *MainToolBar::createToolButton(const QString &emoji, const QString &text)
{
    auto *button = new QToolButton();
    button->setText(emoji + "\n" + text);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setStyleSheet(
        "QToolButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 6px;"
        "    margin: 2px;"
        "    font-size: 10px;"
        "    color: #1d1d1f;"
        "    min-width: 60px;"
        "    max-width: 60px;"
        "    text-align: center;"
        "}"
        "QToolButton:hover { background-color: #f0f0f0; }"
        "QToolButton:pressed { background-color: #e0e0e0; }");
    return button;
}

void MainToolBar::setupActions()
{
    auto *newButton = createToolButton("\U0001F4C4", "New");
    connect(newButton, &QToolButton::clicked, this, &MainToolBar::newClicked);
    newButton->setToolTip("Create new archive (Ctrl+N)");
    addWidget(newButton);

    auto *openButton = createToolButton("\U0001F4C2", "Open");
    connect(openButton, &QToolButton::clicked, this, &MainToolBar::openClicked);
    openButton->setToolTip("Open existing archive (Ctrl+O)");
    addWidget(openButton);

    addSeparator();

    auto *addButton = createToolButton("➕", "Add");
    connect(addButton, &QToolButton::clicked, this, &MainToolBar::addClicked);
    addButton->setToolTip("Add files to archive (Ctrl+A)");
    addWidget(addButton);

    auto *extractButton = createToolButton("\U0001F4E4", "Extract");
    connect(extractButton, &QToolButton::clicked, this, &MainToolBar::extractClicked);
    extractButton->setToolTip("Extract files from archive (Ctrl+E)");
    addWidget(extractButton);

    addSeparator();

    auto *settingsButton = createToolButton("⚙️", "Settings");
    connect(settingsButton, &QToolButton::clicked, this, &MainToolBar::settingsClicked);
    settingsButton->setToolTip("Archive settings and options");
    addWidget(settingsButton);

    auto *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    addLogo();
}

void MainToolBar::addLogo()
{
    auto *logoLabel = new QLabel();
    const QString logoPath = "Assets/zipline Logo.png";
    if (QFile::exists(logoPath)) {
        QPixmap pixmap(logoPath);
        logoLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
    } else {
        logoLabel->setText("Zipline");
        logoLabel->setStyleSheet(
            "QLabel { font-size: 14px; font-weight: bold; color: #007aff; padding: 6px; }");
    }
    logoLabel->setAlignment(Qt::AlignCenter);
    addWidget(logoLabel);
}
