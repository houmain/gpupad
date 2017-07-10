#ifndef GLVERTEX_STREAM_H
#define GLVERTEX_STREAM_H

#include "GLProgram.h"
#include "GLBuffer.h"
#include <QOpenGLVertexArrayObject>

class GLVertexStream
{
public:
    explicit GLVertexStream(const VertexStream &vertexStream);
    void setAttribute(int attributeIndex,
        const Column &column, GLBuffer *buffer);

    void bind(const GLProgram &program);
    void unbind();
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct GLAttribute
    {
        QSet<ItemId> usedItems;
        QString name;
        bool normalize;
        int divisor;
        GLBuffer *buffer;
        Column::DataType type;
        int count;
        int stride;
        int offset;
    };

    QSet<ItemId> mUsedItems;
    QList<GLAttribute> mAttributes;
    QOpenGLVertexArrayObject mVertexArrayObject;
    QList<GLuint> mEnabledVertexAttributes;
};

#endif // GLVERTEX_STREAM_H
