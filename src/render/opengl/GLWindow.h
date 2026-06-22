#pragma once
#if defined(OPENGL_ENABLED)

#  include <QWindow>
#  include "GLContext.h"

class GLWindow final : public QWindow
{
    Q_OBJECT
public:
    static AdapterIdentity getAdapterIdentity();

    explicit GLWindow(bool enableVSync = false, QWindow *parent = nullptr);
    explicit GLWindow(QWindow *parent) = delete;
    ~GLWindow() override;

    bool initialized() const { return (mGL && mGL->initialized()); }
    GLContext &gl() { return *mGL; }
    bool makeCurrent();
    void redraw();

Q_SIGNALS:
    void initializingGpu();
    void paintingGpu();
    void releasingGpu();

private:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void releaseGpu();

    const bool mEnableVSync;
    std::unique_ptr<QOpenGLContext> mContext;
    bool mInitialized{};
    bool mReleasing{};
    std::unique_ptr<GLContext> mGL;
};

#endif // defined(OPENGL_ENABLED)
