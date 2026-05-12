#pragma once

#include <QWindow>

class RenderWindow : public QWindow
{
    Q_OBJECT
public:
    using QWindow::QWindow;
    ~RenderWindow() override = default;

    virtual bool initialized() const = 0;
    virtual void update() = 0;
    using QWindow::requestUpdate;

Q_SIGNALS:
    void initializingGpu();
    void paintingGpu();
    void releasingGpu();
};
