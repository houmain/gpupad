#pragma once
#if defined(OPENGL_ENABLED)

#  include "GLDevice.h"
#  include "render/RenderWindow.h"
#  include <QOpenGLVertexArrayObject>
#  include <chrono>

class GLWindow : public RenderWindow
{
    Q_OBJECT
public:
    static AdapterIdentity getAdapterIdentity();

    explicit GLWindow(int syncInterval = 0);
    ~GLWindow() override;

    bool initialized() const override { return mInitialized; }
    QOpenGLFunctions_4_5_Core &gl() { return mDevice->gl(); }

    void update() override;
    bool makeCurrent();

private:
    void releaseGL();
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initializeGL();
    void paintGL();

    const int mSyncInterval{};
    bool mInitialized{};
    std::unique_ptr<GLDevice> mDevice;
    QOpenGLVertexArrayObject mVao;
    std::chrono::high_resolution_clock::time_point mLastSwapTime{};
};

#endif // defined(OPENGL_ENABLED)
