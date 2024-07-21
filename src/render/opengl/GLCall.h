#pragma once

#include "GLItem.h"
#include "scripting/ScriptEngine.h"

class QOpenGLTimerQuery;
class GLProgram;
class GLTarget;
class GLStream;
class GLBuffer;
class GLTexture;
using TimerQueryPtr = std::shared_ptr<const QOpenGLTimerQuery>;

class GLCall
{
public:
    explicit GLCall(const Call &call);

    ItemId itemId() const { return mCall.id; }
    GLProgram *program() { return mProgram; }
    TimerQueryPtr timerQuery() const { return mTimerQuery; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

    void setProgram(GLProgram *program);
    void setTarget(GLTarget *target);
    void setVextexStream(GLStream *vertexStream);
    void setIndexBuffer(GLBuffer *indices, const Block &block);
    void setIndirectBuffer(GLBuffer *commands, const Block &block);
    void setBuffers(GLBuffer *buffer, GLBuffer *fromBuffer);
    void setTextures(GLTexture *texture, GLTexture *fromTexture);
    void execute(MessagePtrSet &messages, ScriptEngine &scriptEngine);

private:
    std::shared_ptr<void> beginTimerQuery();
    int evaluateInt(ScriptEngine &scriptEngine, const QString &expression);
    void executeDraw(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeCompute(MessagePtrSet &messages, ScriptEngine &scriptEngine);
    void executeClearTexture(MessagePtrSet &messages);
    void executeCopyTexture(MessagePtrSet &messages);
    void executeClearBuffer(MessagePtrSet &messages);
    void executeCopyBuffer(MessagePtrSet &messages);
    void executeSwapTextures(MessagePtrSet &messages);
    void executeSwapBuffers(MessagePtrSet &messages);
    GLenum getIndexType() const;

    MessagePtrSet mMessages;
    Call mCall{ };
    GLProgram *mProgram{ };
    GLTarget *mTarget{ };
    GLStream *mVertexStream{ };
    GLBuffer *mBuffer{ };
    GLBuffer *mFromBuffer{ };
    GLTexture *mTexture{ };
    GLTexture *mFromTexture{ };

    GLBuffer *mIndexBuffer{ };
    QString mIndirectOffset;
    int mIndexSize{ };
    QString mIndicesOffset;

    GLBuffer *mIndirectBuffer{ };
    GLint mIndirectStride{ };

    QSet<ItemId> mUsedItems;
    std::shared_ptr<QOpenGLTimerQuery> mTimerQuery;
};

