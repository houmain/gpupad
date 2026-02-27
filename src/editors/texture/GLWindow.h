#pragma once

#include <QWindow>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>

class QOpenGLContext;

class GLWindow : public QWindow, protected QOpenGLFunctions_4_5_Core
{
    Q_OBJECT
public:
    explicit GLWindow(QWindow *parent = nullptr);
    ~GLWindow();
    QOpenGLFunctions_4_5_Core &gl() { return *this; }

    bool initialized() const { return mInitialized; }
    void update();
    using QWindow::requestUpdate;

Q_SIGNALS:
    void initializingGL();
    void paintingGL();
    void releasingGL();

private:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void initialize();
    void handleDebugMessage(const QOpenGLDebugMessage &message);

    bool mInitialized{};
    QOpenGLContext *m_context{};
    QOpenGLVertexArrayObject mVao;
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
};
