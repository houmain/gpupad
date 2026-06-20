#pragma once
#if defined(OPENGL_ENABLED)

#  include "GLContext.h"
#  include "render/GpuWindow.h"
#  include <chrono>
#  include <memory>

class GLWindow : public GpuWindow
{
    Q_OBJECT
public:
    static AdapterIdentity getAdapterIdentity();

    explicit GLWindow(int syncInterval = 0);
    ~GLWindow() override;

    bool initialized() const override { return static_cast<bool>(mContext); }
    GLContext &context() { return *mContext; }

    void update() override;
    bool makeCurrent();

private:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initializeGL();
    void paintGL();

    const int mSyncInterval{};
    std::unique_ptr<GLContext> mContext;
    std::chrono::high_resolution_clock::time_point mLastSwapTime{};
};

#endif // defined(OPENGL_ENABLED)
