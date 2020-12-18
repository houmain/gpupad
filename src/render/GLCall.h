#ifndef GLCALL_H
#define GLCALL_H

#include "GLItem.h"
#include "scripting/ScriptEngine.h"
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
    GLCall(const Call &call, ScriptEngine &scriptEngine, MessagePtrSet &messages);

    ItemId itemId() const { return mCall.id; }
    GLProgram *program() { return mProgram; }
    std::shared_ptr<const QOpenGLTimerQuery> timerQuery() const { return mTimerQuery; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    void setProgram(GLProgram *program);
    void setTarget(GLTarget *target);
    void setVextexStream(GLStream *vertexStream);
    void setIndexBuffer(GLBuffer *indices, const Block &block);
    void setIndirectBuffer(GLBuffer *commands, const Block &block,
        ScriptEngine &scriptEngine, MessagePtrSet &messages);
    void setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer);
    void setTextures(GLTexture *texture, GLTexture *fromTexture);
    void execute(MessagePtrSet &messages);

private:
    std::shared_ptr<void> beginTimerQuery();
    void executeDraw(MessagePtrSet &messages);
    void executeCompute(MessagePtrSet &messages);
    void executeClearTexture(MessagePtrSet &messages);
    void executeCopyTexture(MessagePtrSet &messages);
    void executeClearBuffer(MessagePtrSet &messages);
    void executeCopyBuffer(MessagePtrSet &messages);
    GLenum getIndexType() const;

    Call mCall{ };
    GLProgram *mProgram{ };
    GLTarget *mTarget{ };
    GLStream *mVertexStream{ };
    GLBuffer *mBuffer{ };
    GLBuffer *mFromBuffer{ };
    GLTexture *mTexture{ };
    GLTexture *mFromTexture{ };

    GLBuffer *mIndexBuffer{ };
    GLuint mIndirectOffset{ };
    int mIndexSize{ };

    GLBuffer *mIndirectBuffer{ };
    GLint mIndirectStride{ };

    ScriptVariable mFirst;
    ScriptVariable mCount;
    ScriptVariable mInstanceCount;
    ScriptVariable mBaseVertex;
    ScriptVariable mBaseInstance;
    ScriptVariable mDrawCount;
    ScriptVariable mPatchVertices;
    ScriptVariable mWorkgroupsX;
    ScriptVariable mWorkgroupsY;
    ScriptVariable mWorkgroupsZ;

    QSet<ItemId> mUsedItems;
    std::shared_ptr<QOpenGLTimerQuery> mTimerQuery;
};

#endif // GLCALL_H
