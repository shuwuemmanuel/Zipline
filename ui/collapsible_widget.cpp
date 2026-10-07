#include "collapsible_widget.h"

CollapsibleBox::CollapsibleBox(const QString &title, QWidget *parent)
    : QWidget(parent), base_title_(title)
{
    toggle_button_ = new QPushButton(title);
    toggle_button_->setCheckable(true);
    toggle_button_->setChecked(false);
    connect(toggle_button_, &QPushButton::clicked, this, &CollapsibleBox::toggle);

    content_area_ = new QFrame();
    content_area_->setMaximumHeight(0);
    content_area_->setMinimumHeight(0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toggle_button_);
    layout->addWidget(content_area_);

    animation_ = new QPropertyAnimation(content_area_, "maximumHeight");
    animation_->setDuration(200);

    setupButtonStyle();
    updateArrow();
}

void CollapsibleBox::setupButtonStyle()
{
    toggle_button_->setStyleSheet(
        "QPushButton {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #d2d2d7;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    text-align: left;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "    color: #1d1d1f;"
        "}"
        "QPushButton:hover { background-color: #e9ecef; }"
        "QPushButton:checked { background-color: #007aff; color: white; }"
        "QPushButton::indicator { width: 0px; height: 0px; }");
}

void CollapsibleBox::updateArrow()
{
    const QString arrow = toggle_button_->isChecked() ? "▼ " : "▶ ";
    toggle_button_->setText(arrow + base_title_);
}

void CollapsibleBox::setContentLayout(QLayout *layout)
{
    content_area_->setLayout(layout);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    content_height_ = layout->sizeHint().height();
}

void CollapsibleBox::toggle()
{
    updateArrow();
    if (toggle_button_->isChecked()) {
        animation_->setStartValue(0);
        animation_->setEndValue(content_height_);
    } else {
        animation_->setStartValue(content_height_);
        animation_->setEndValue(0);
    }
    animation_->start();
}

void CollapsibleBox::expand()
{
    if (!toggle_button_->isChecked()) {
        toggle_button_->setChecked(true);
        toggle();
    }
}

void CollapsibleBox::collapse()
{
    if (toggle_button_->isChecked()) {
        toggle_button_->setChecked(false);
        toggle();
    }
}

CollapsibleGroup::CollapsibleGroup(QWidget *parent) : QWidget(parent)
{
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(2);
}

CollapsibleBox *CollapsibleGroup::addBox(const QString &title, QLayout *contentLayout)
{
    auto *box = new CollapsibleBox(title, this);
    box->setContentLayout(contentLayout);
    connect(box->toggleButton(), &QPushButton::clicked, this,
            [this, box](bool checked) { onBoxToggled(box, checked); });
    boxes_.push_back(box);
    layout_->addWidget(box);
    return box;
}

void CollapsibleGroup::onBoxToggled(CollapsibleBox *toggledBox, bool checked)
{
    if (checked) {
        for (auto *box : boxes_)
            if (box != toggledBox && box->toggleButton()->isChecked())
                box->collapse();
    }
}

void CollapsibleGroup::expandFirst()
{
    if (!boxes_.empty())
        boxes_.front()->expand();
}
