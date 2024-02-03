#pragma once

#include "../RenderSessionBase.h"

class VKRenderer;
class ScriptSession;

class VKRenderSession final : public RenderSessionBase
{
public:
    explicit VKRenderSession(VKRenderer &renderer);
    ~VKRenderSession();

    void render() override;
    void finish() override;
    void release() override;

    bool updatingPreviewTextures() const override {
      // TODO: implement texture sharing with preview
      return false;
    }

private:
    struct CommandQueue;
    struct GroupIteration;

    void createCommandQueue();
    void reuseUnmodifiedItems();
    void executeCommandQueue();
    void setNextCommandQueueIndex(int index);
    void downloadModifiedResources();
    void outputTimerQueries();

    VKRenderer &mRenderer;
    QScopedPointer<CommandQueue> mCommandQueue;
    QScopedPointer<CommandQueue> mPrevCommandQueue;
    int mNextCommandQueueIndex{ };
    QMap<ItemId, GroupIteration> mGroupIterations;
    //QList<std::pair<ItemId, std::shared_ptr<const QOpenGLTimerQuery>>> mTimerQueries;
    MessagePtrSet mTimerMessages;
};
