
#include "TitleBar.h"
#include <QDockWidget>
#include <QLabel>
#include <QBoxLayout>
#include <QToolButton>

TitleBar::TitleBar(QDockWidget *parent) 
    : QFrame(parent)
{
    setFrameShape(QFrame::Box);
    setProperty("no-bottom-border", true);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto title = new QLabel(parent->windowTitle(), this);
    layout->addWidget(title);

    layout->addStretch(10);

    auto closeButton = new QToolButton(this);
    closeButton->setObjectName("fileNew");
    QIcon icon(QIcon::fromTheme(QString::fromUtf8(":images/dock-close.png")));
    closeButton->setIcon(icon);
    closeButton->setAutoRaise(true);
    closeButton->setMaximumSize(16, 16);
    layout->addWidget(closeButton);

    connect(closeButton, &QToolButton::clicked, parent, &QDockWidget::close);
}
