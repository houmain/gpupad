#pragma once

#include <QWindow>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLDebugLogger>
#include <chrono>

class QOpenGLContext;

class GLWindow : public QWindow
{
    Q_OBJECT
public:
    GLWindow();
    explicit GLWindow(int syncInterval);
    ~GLWindow();

    bool initialized() const { return mInitialized; }
    QOpenGLFunctions_4_5_Core &gl() { return mGL; }

    void update();
    using QWindow::requestUpdate;

Q_SIGNALS:
    void initializingGL();
    void paintingGL();
    void releasingGL();

private:
    void handleDebugMessage(const QOpenGLDebugMessage &message);
    void releaseGL();
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initializeGL();
    void paintGL();

    const int mSyncInterval{};
    bool mInitialized{};
    QOpenGLContext *mContext{};
    QOpenGLFunctions_4_5_Core mGL;
    QOpenGLVertexArrayObject mVao;
    std::chrono::high_resolution_clock::time_point mLastSwapTime{ };
#if !defined(NDEBUG)
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
#endif
};
