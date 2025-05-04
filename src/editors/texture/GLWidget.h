#pragma once

#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_2_Core>
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

    QOpenGLFunctions_3_3_Core &gl();
    QOpenGLFunctions_4_2_Core *gl42();
    QOpenGLFunctions_4_5_Core *gl45();

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
    QOpenGLFunctions_3_3_Core mGL;
    std::optional<QOpenGLFunctions_4_2_Core> mGL42;
    std::optional<QOpenGLFunctions_4_5_Core> mGL45;
    QOpenGLVertexArrayObject mVao;
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
};
