#pragma once

#include <QFrame>

class QDockWidget;

class TitleBar : public QFrame
{
    Q_OBJECT

public:
    explicit TitleBar(QDockWidget *parent);
};
