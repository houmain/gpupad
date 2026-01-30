#pragma once

#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <optional>

class GLWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent);
    ~GLWidget();

    bool initialized() const { return mInitialized; }
    QOpenGLFunctions_4_5_Core &gl();

    using QOpenGLWidget::paintEvent;

Q_SIGNALS:
    void initializingGL();
    void paintingGL();
    void releasingGL();

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void handleDebugMessage(const QOpenGLDebugMessage &message);
    void releaseGL();

    bool mInitialized{};
    QOpenGLFunctions_4_5_Core mGL;
    QOpenGLVertexArrayObject mVao;
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
};
