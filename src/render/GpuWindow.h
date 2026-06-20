#pragma once

#include <QWindow>

class GpuWindow : public QWindow
{
    Q_OBJECT
public:
    using QWindow::QWindow;
    ~GpuWindow() override = default;

    virtual bool initialized() const = 0;
    virtual void update() = 0;
    using QWindow::requestUpdate;

Q_SIGNALS:
    void initializingGpu();
    void preparingGpu();
    void paintingGpu();
    void submittedGpu();
    void releasingGpu();
};
