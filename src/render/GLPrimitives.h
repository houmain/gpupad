#ifndef GLPRIMITIVES_H
#define GLPRIMITIVES_H

#include "GLProgram.h"
#include "GLBuffer.h"
#include <QOpenGLVertexArrayObject>

class GLPrimitives
{
public:
    explicit GLPrimitives(const Primitives &primitives);
    void setAttribute(int attributeIndex,
        const Column &column, GLBuffer *buffer);
    void setIndices(const Column &column, GLBuffer *indices);

    void draw(const GLProgram &program);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct GLAttribute
    {
        ItemId id;
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
    std::vector<GLAttribute> mAttributes;
    Primitives::Type mType{ };
    int mFirstVertex{ };
    int mVertexCount{ };
    int mPatchVertices{ };
    int mInstanceCount{ };
    int mPrimitiveRestartIndex{ };
    GLBuffer *mIndexBuffer{ };
    GLenum mIndexType{ };
    int mIndicesOffset{ };
    QOpenGLVertexArrayObject mVertexArrayObject;
};

#endif // GLPRIMITIVES_H
