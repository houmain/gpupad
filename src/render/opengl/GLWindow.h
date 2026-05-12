#pragma once

#include "render/RenderWindow.h"
#include "GLContext.h"
#include <QWindow>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLDebugLogger>
#include <chrono>

class GLWindow : public RenderWindow
{
    Q_OBJECT
public:
    explicit GLWindow(int syncInterval = 0);
    ~GLWindow() override;

    bool initialized() const override { return mInitialized; }
    QOpenGLFunctions_4_5_Core &gl() { return *mContext; }

    void update() override;
    bool makeCurrent();

private:
    void handleDebugMessage(const QOpenGLDebugMessage &message);
    void releaseGL();
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initializeGL();
    void paintGL();

    const int mSyncInterval{};
    bool mInitialized{};
    GLContext *mContext{};
    QOpenGLVertexArrayObject mVao;
    std::chrono::high_resolution_clock::time_point mLastSwapTime{ };
#if !defined(NDEBUG)
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
#endif
};
