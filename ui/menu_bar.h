/* Menu Bar for Zipline Archive Manager (C++ port of ui/menu_bar.py) */
#ifndef ZIPLINE_MENU_BAR_H
#define ZIPLINE_MENU_BAR_H

#include <QMenuBar>
#include <QAction>

class MainMenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit MainMenuBar(QWidget *parent = nullptr);

    QAction *newArchive = nullptr;
    QAction *openArchive = nullptr;
    QAction *addFiles = nullptr;
    QAction *addFolder = nullptr;
    QMenu   *recentMenu = nullptr;
    QAction *exitAction = nullptr;

    QAction *selectAll = nullptr;
    QAction *selectNone = nullptr;
    QAction *invertSelection = nullptr;
    QAction *deleteFiles = nullptr;

    QAction *extractAll = nullptr;
    QAction *extractSelected = nullptr;
    QAction *testArchive = nullptr;
    QAction *viewArchiveInfo = nullptr;

    QAction *options = nullptr;
    QAction *benchmark = nullptr;

    QAction *toggleToolbar = nullptr;
    QAction *toggleStatusbar = nullptr;
    QAction *toggleSettingsPanel = nullptr;
    QAction *refresh = nullptr;

    QAction *helpAction = nullptr;
    QAction *aboutAction = nullptr;

private:
    void setupMenus();
};

#endif
