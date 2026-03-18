#pragma once

#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QWindow>

class QOpenGLContext;

class GLWindow : public QWindow
{
    Q_OBJECT
public:
    explicit GLWindow();
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

    bool mInitialized{};
    QOpenGLContext *mContext{};
    QOpenGLFunctions_4_5_Core mGL;
    QOpenGLVertexArrayObject mVao;
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
};
