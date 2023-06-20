
#include "WindowTitle.h"
#include <QDockWidget>
#include <QLabel>
#include <QBoxLayout>
#include <QToolButton>
#include <QEvent>

WindowTitle::WindowTitle(QWidget *parent) 
    : QFrame(parent)
{
    setFrameShape(QFrame::Box);
    setProperty("no-bottom-border", true);

    auto layout = new QVBoxLayout(this);
    auto widget = new QWidget();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget);

    auto titleLayout = new QHBoxLayout(widget);
    titleLayout->setContentsMargins(4, 4, 4, 4);
    mTitle = new QLabel(this);
    titleLayout->addWidget(mTitle);

    titleLayout->addStretch(1);

    auto closeButton = new QToolButton(this);
    closeButton->setObjectName("fileNew");
    QIcon icon(QIcon::fromTheme(QString::fromUtf8(":images/dock-close.png")));
    closeButton->setIcon(icon);
    closeButton->setAutoRaise(true);
    closeButton->setMaximumSize(16, 16);
    titleLayout->addWidget(closeButton);
    mCloseButton = closeButton;

    parentChanged(parent);
}

bool WindowTitle::event(QEvent *event)
{
    if (event->type() == QEvent::ParentAboutToChange) {
        if (auto parent = parentWidget()) 
            disconnect(parent);
    }
    else if (event->type() == QEvent::ParentChange) {
        parentChanged(parentWidget());
    }
    return QFrame::event(event);
}

void WindowTitle::parentChanged(QWidget *parent) {
    if (!parent)
        return;

    connect(mCloseButton, &QToolButton::clicked, parent, &QWidget::close);
    connect(parent, &QWidget::windowTitleChanged, mTitle, &QLabel::setText);
    mTitle->setText(parent->windowTitle());
}

void WindowTitle::setWidget(QWidget *widget)
{
    if (mWidget)
        layout()->removeWidget(mWidget);

    mWidget = widget;

    if (widget) {
        widget->setParent(this);
        layout()->addWidget(widget);
    }
}
