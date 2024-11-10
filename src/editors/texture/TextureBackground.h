#pragma once

#include <QObject>

class GLWidget;
class QOpenGLShaderProgram;

class TextureBackground final : public QObject
{
    Q_OBJECT
public:
    explicit TextureBackground(GLWidget *parent);
    ~TextureBackground() override;
    void releaseGL();
    void paintGL(const QSizeF &size, const QPointF &offset);

private:
    GLWidget &widget();
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
};
