#pragma once

#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>

#if defined(USE_WINDOW_CONTAINER)
#  include <QWindow>
using GLWindowBase = QWindow;
#else
#  include <QOpenGLWidget>
using GLWindowBase = QOpenGLWidget;
#endif

class QOpenGLContext;

class GLWindow : public GLWindowBase
{
    Q_OBJECT
public:
    explicit GLWindow();
    ~GLWindow();

    bool initialized() const { return mInitialized; }
    QOpenGLFunctions_4_5_Core &gl() { return mGL; }

#if defined(USE_WINDOW_CONTAINER)
    void paintEvent(QPaintEvent *) { update(); }
    void update();
    using GLWindowBase::requestUpdate;
#else
    using QOpenGLWidget::paintEvent;
    void requestUpdate() { update(); }
#endif

Q_SIGNALS:
    void initializingGL();
    void paintingGL();
    void releasingGL();

private:
    void handleDebugMessage(const QOpenGLDebugMessage &message);
    void releaseGL();

#if defined(USE_WINDOW_CONTAINER)
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initializeGL();
    void paintGL();
#else
    void initializeGL() override;
    void paintGL() override;
#endif

    bool mInitialized{};
    QOpenGLContext *m_context{};
    QOpenGLFunctions_4_5_Core mGL;
    QOpenGLVertexArrayObject mVao;
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
};
