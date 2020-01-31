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
class ScriptEngine;

class GLCall
{
public:
    explicit GLCall(const Call &call);

    ItemId itemId() const { return mCall.id; }
    GLProgram *program() { return mProgram; }
    void setProgram(GLProgram *program);
    void setTarget(GLTarget *target);
    void setVextexStream(GLStream *vertexStream);
    void setIndexBuffer(GLBuffer *indices, const Buffer &buffer);
    void setIndirectBuffer(GLBuffer *commands, const Buffer &buffer);
    void setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer);
    void setTextures(GLTexture *texture, GLTexture *fromTexture);

    void execute(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    std::shared_ptr<const QOpenGLTimerQuery> timerQuery() const { return mTimerQuery; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    std::shared_ptr<void> beginTimerQuery();
    void executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeClearTexture(MessagePtrSet &messages);
    void executeCopyTexture(MessagePtrSet &messages);
    void executeClearBuffer(MessagePtrSet &messages);
    void executeCopyBuffer(MessagePtrSet &messages);
    void executeGenerateMipmaps(MessagePtrSet &messages);

    Call mCall{ };
    GLProgram *mProgram{ };
    GLTarget *mTarget{ };
    GLStream *mVertexStream{ };
    GLBuffer *mBuffer{ };
    GLBuffer *mFromBuffer{ };
    GLTexture *mTexture{ };
    GLTexture *mFromTexture{ };

    GLBuffer *mIndexBuffer{ };
    GLuint mIndexType{ };
    int mIndexSize{ };

    GLBuffer *mIndirectBuffer{ };
    uintptr_t mIndirectOffset{ };
    GLint mIndirectStride{ };

    QSet<ItemId> mUsedItems;
    std::shared_ptr<QOpenGLTimerQuery> mTimerQuery;
};

#endif // GLCALL_H
