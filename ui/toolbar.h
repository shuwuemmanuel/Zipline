/* Toolbar for Zipline Archive Manager (C++ port of ui/toolbar.py) */
#ifndef ZIPLINE_TOOLBAR_H
#define ZIPLINE_TOOLBAR_H

#include <QToolBar>
#include <QToolButton>

class MainToolBar : public QToolBar {
    Q_OBJECT
public:
    explicit MainToolBar(QWidget *parent = nullptr);

signals:
    void newClicked();
    void openClicked();
    void addClicked();
    void extractClicked();
    void settingsClicked();

private:
    void setupActions();
    QToolButton *createToolButton(const QString &emoji, const QString &text);
    void addLogo();
};

#endif
