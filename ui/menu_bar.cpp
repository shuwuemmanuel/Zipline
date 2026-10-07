#include "menu_bar.h"

#include <QMenu>
#include <QKeySequence>

MainMenuBar::MainMenuBar(QWidget *parent) : QMenuBar(parent)
{
    setupMenus();
}

void MainMenuBar::setupMenus()
{
    // File menu
    QMenu *fileMenu = addMenu("&File");

    newArchive = new QAction("&New Archive", this);
    newArchive->setShortcut(QKeySequence::New);
    newArchive->setStatusTip("Create a new archive");
    fileMenu->addAction(newArchive);

    openArchive = new QAction("&Open Archive...", this);
    openArchive->setShortcut(QKeySequence::Open);
    openArchive->setStatusTip("Open an existing archive");
    fileMenu->addAction(openArchive);

    fileMenu->addSeparator();

    addFiles = new QAction("&Add Files...", this);
    addFiles->setShortcut(QKeySequence("Ctrl+A"));
    addFiles->setStatusTip("Add files to the current archive");
    fileMenu->addAction(addFiles);

    addFolder = new QAction("Add &Folder...", this);
    addFolder->setShortcut(QKeySequence("Ctrl+F"));
    addFolder->setStatusTip("Add a folder to the current archive");
    fileMenu->addAction(addFolder);

    fileMenu->addSeparator();

    recentMenu = fileMenu->addMenu("&Recent Archives");

    fileMenu->addSeparator();

    exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    if (parent())
        connect(exitAction, &QAction::triggered, parentWidget(), &QWidget::close);
    fileMenu->addAction(exitAction);

    // Edit menu
    QMenu *editMenu = addMenu("&Edit");

    selectAll = new QAction("Select &All", this);
    selectAll->setShortcut(QKeySequence::SelectAll);
    selectAll->setStatusTip("Select all files in the archive");
    editMenu->addAction(selectAll);

    selectNone = new QAction("Select &None", this);
    selectNone->setShortcut(QKeySequence("Ctrl+D"));
    selectNone->setStatusTip("Deselect all files");
    editMenu->addAction(selectNone);

    invertSelection = new QAction("&Invert Selection", this);
    invertSelection->setShortcut(QKeySequence("Ctrl+I"));
    invertSelection->setStatusTip("Invert the current selection");
    editMenu->addAction(invertSelection);

    editMenu->addSeparator();

    deleteFiles = new QAction("&Delete Selected", this);
    deleteFiles->setShortcut(QKeySequence::Delete);
    deleteFiles->setStatusTip("Delete selected files from the archive");
    editMenu->addAction(deleteFiles);

    // Actions menu
    QMenu *actionsMenu = addMenu("&Actions");

    extractAll = new QAction("&Extract All...", this);
    extractAll->setShortcut(QKeySequence("Ctrl+E"));
    extractAll->setStatusTip("Extract all files from the archive");
    actionsMenu->addAction(extractAll);

    extractSelected = new QAction("Extract &Selected...", this);
    extractSelected->setShortcut(QKeySequence("Ctrl+Shift+E"));
    extractSelected->setStatusTip("Extract selected files from the archive");
    actionsMenu->addAction(extractSelected);

    actionsMenu->addSeparator();

    testArchive = new QAction("&Test Archive", this);
    testArchive->setShortcut(QKeySequence("Ctrl+T"));
    testArchive->setStatusTip("Test archive integrity");
    actionsMenu->addAction(testArchive);

    viewArchiveInfo = new QAction("Archive &Information", this);
    viewArchiveInfo->setStatusTip("View detailed archive information");
    actionsMenu->addAction(viewArchiveInfo);

    // Tools menu
    QMenu *toolsMenu = addMenu("&Tools");

    options = new QAction("&Options...", this);
    options->setStatusTip("Configure application settings");
    toolsMenu->addAction(options);

    benchmark = new QAction("&Benchmark", this);
    benchmark->setStatusTip("Run compression benchmark");
    toolsMenu->addAction(benchmark);

    // View menu
    QMenu *viewMenu = addMenu("&View");

    toggleToolbar = new QAction("&Toolbar", this);
    toggleToolbar->setCheckable(true);
    toggleToolbar->setChecked(true);
    toggleToolbar->setStatusTip("Show/hide toolbar");
    viewMenu->addAction(toggleToolbar);

    toggleStatusbar = new QAction("&Status Bar", this);
    toggleStatusbar->setCheckable(true);
    toggleStatusbar->setChecked(true);
    toggleStatusbar->setStatusTip("Show/hide status bar");
    viewMenu->addAction(toggleStatusbar);

    toggleSettingsPanel = new QAction("Settings &Panel", this);
    toggleSettingsPanel->setCheckable(true);
    toggleSettingsPanel->setChecked(true);
    toggleSettingsPanel->setStatusTip("Show/hide settings panel");
    viewMenu->addAction(toggleSettingsPanel);

    viewMenu->addSeparator();

    refresh = new QAction("&Refresh", this);
    refresh->setShortcut(QKeySequence::Refresh);
    refresh->setStatusTip("Refresh the current view");
    viewMenu->addAction(refresh);

    // Help menu
    QMenu *helpMenu = addMenu("&Help");

    helpAction = new QAction("&Help", this);
    helpAction->setShortcut(QKeySequence::HelpContents);
    helpAction->setStatusTip("Show help documentation");
    helpMenu->addAction(helpAction);

    helpMenu->addSeparator();

    aboutAction = new QAction("&About Zipline", this);
    aboutAction->setStatusTip("About this application");
    helpMenu->addAction(aboutAction);
}
