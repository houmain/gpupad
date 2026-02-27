#pragma once

#include <QObject>

class GLWindow;
class QOpenGLShaderProgram;

class TextureBackground final : public QObject
{
    Q_OBJECT
public:
    explicit TextureBackground(GLWindow *parent);
    ~TextureBackground() override;
    void releaseGL();
    void paintGL(const QSizeF &size, const QPointF &offset);

private:
    GLWindow &window();
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
};
