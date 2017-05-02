#ifndef RENDERCALL_H
#define RENDERCALL_H

#include "RenderTask.h"
#include "MessageList.h"
#include <QScopedPointer>
#include <QSet>

class SessionModel;

class RenderCall : public RenderTask
{
public:
    struct Input;
    struct Cache;
    struct Output;

    explicit RenderCall(ItemId callId, QObject *parent = nullptr);
    ~RenderCall();

    QSet<ItemId> usedItems() const override { return mUsedItems; }

private:
    void prepare() override;
    void render(QOpenGLContext &glContext) override;
    void finish() override;
    void release(QOpenGLContext &glContext) override;

private:
    ItemId mCallId;
    QSet<ItemId> mUsedItems;
    MessageList mMessages;
    QScopedPointer<Input> mInput;
    QScopedPointer<Cache> mCache;
    QScopedPointer<Output> mOutput;
};

#endif // RENDERCALL_H

