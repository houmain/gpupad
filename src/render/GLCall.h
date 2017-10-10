#ifndef GLCALL_H
#define GLCALL_H

#include "GLItem.h"
#include <chrono>

class QOpenGLTimerQuery;
class GLProgram;
class GLTarget;
class GLStream;
class GLBuffer;
class GLTexture;

class GLCall
{
public:
    explicit GLCall(const Call& call);

    ItemId itemId() const { return mCall.id; }
    GLProgram *program() { return mProgram; }
    void setProgram(GLProgram *program);
    void setTarget(GLTarget *target);
    void setVextexStream(GLStream *vertexStream);
    void setIndexBuffer(GLBuffer *indices, const Buffer &buffer);
    void setIndirectBuffer(GLBuffer *commands, const Buffer &buffer);
    void setBuffer(GLBuffer *buffer);
    void setTexture(GLTexture *texture);

    void execute();
    std::chrono::nanoseconds duration() const;
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    QOpenGLTimerQuery &timerQuery();
    void executeDraw();
    void executeCompute();
    void executeClearTexture();
    void executeClearBuffer();
    void executeGenerateMipmaps();

    Call mCall{ };
    GLProgram *mProgram{ };
    GLTarget *mTarget{ };
    GLStream *mVertexStream{ };
    GLBuffer *mBuffer{ };
    GLTexture *mTexture{ };

    GLBuffer *mIndexBuffer{ };
    GLuint mIndexType{ };
    uintptr_t mIndicesOffset{ };

    GLBuffer *mIndirectBuffer{ };
    uintptr_t mIndirectOffset{ };
    GLint mIndirectStride{ };

    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    std::shared_ptr<QOpenGLTimerQuery> mTimerQuery;
};

#endif // GLCALL_H
