#pragma once

#include <QFrame>

class QDockWidget;
class QAbstractButton;
class QLabel;

class WindowTitle : public QFrame
{
    Q_OBJECT

public:
    explicit WindowTitle(QWidget *parent = nullptr);

    void setWidget(QWidget *widget);
    QWidget *widget() const { return mWidget; }

protected:
    bool event(QEvent *event) override;

private:
    void parentChanged(QWidget *parent);

    QLabel *mTitle{};
    QAbstractButton *mCloseButton{};
    QWidget *mWidget{};
};
