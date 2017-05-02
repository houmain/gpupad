#ifndef GLPRIMITIVES_H
#define GLPRIMITIVES_H

#include "GLProgram.h"
#include "GLBuffer.h"
#include <QOpenGLVertexArrayObject>

class GLPrimitives
{
public:
    void initialize(PrepareContext &context, const Primitives &primitives);

    void cache(RenderContext &context, GLPrimitives &&update);
    void draw(RenderContext &context, const GLProgram &program);

private:
    struct GLAttribute {
        int bufferIndex;
        QString name;
        int count;
        Column::DataType type;
        bool normalize;
        int stride;
        int offset;
        int divisor;
    };

    QOpenGLVertexArrayObject mVertexArrayObject;
    std::vector<GLBuffer> mBuffers;
    std::vector<GLAttribute> mAttributes;
    Primitives::Type mType{ };
    int mFirstVertex{ };
    int mVertexCount{ };
    int mPatchVertices{ };
    int mInstanceCount{ };
    int mPrimitiveRestartIndex{ };
    int mIndexBufferIndex{ -1 };
    GLenum mIndexType{ };
    int mIndexSize{ };
};

#endif // GLPRIMITIVES_H
