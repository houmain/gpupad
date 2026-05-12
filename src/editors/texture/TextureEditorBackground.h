#pragma once

#include <QObject>

class RenderWindow;

class TextureEditorBackground : public QObject
{
    Q_OBJECT
public:
    explicit TextureEditorBackground(RenderWindow *parent);
    virtual void releaseGpu() = 0;
    virtual void paintGpu(const QSizeF &size, const QPointF &offset) = 0;

protected:
    struct Color
    {
        float r, g, b, a;
    };

    struct Params
    {
        Color color0;
        Color color1;
        Color lineColor;
        float offsetX{};
        float offsetY{};
        float width{};
        float height{};
    };

    static QString vertexShaderSource;
    static QString fragmentShaderSource;
    static Params getParams(const QSizeF &size, const QPointF &offset);
};
