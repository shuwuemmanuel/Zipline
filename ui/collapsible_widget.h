/*
 * Collapsible Widget for Zipline Archive Manager
 * Creates expandable/collapsible sections like dropdowns.
 * (C++ port of ui/collapsible_widget.py)
 */
#ifndef ZIPLINE_COLLAPSIBLE_WIDGET_H
#define ZIPLINE_COLLAPSIBLE_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QPropertyAnimation>
#include <vector>

class CollapsibleBox : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleBox(const QString &title = "", QWidget *parent = nullptr);
    void setContentLayout(QLayout *layout);
    void expand();
    void collapse();
    QPushButton *toggleButton() { return toggle_button_; }

private slots:
    void toggle();

private:
    void setupButtonStyle();
    void updateArrow();

    QPushButton *toggle_button_;
    QFrame *content_area_;
    QPropertyAnimation *animation_;
    int content_height_ = 0;
    QString base_title_;
};

class CollapsibleGroup : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleGroup(QWidget *parent = nullptr);
    CollapsibleBox *addBox(const QString &title, QLayout *contentLayout);
    void expandFirst();

private slots:
    void onBoxToggled(CollapsibleBox *box, bool checked);

private:
    QVBoxLayout *layout_;
    std::vector<CollapsibleBox *> boxes_;
};

#endif
