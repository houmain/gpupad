#pragma once

#include "RenderTask.h"
#include "RenderSessionBase.h"

class RenderSession final : public RenderTask
{
    Q_OBJECT
public:
    explicit RenderSession(QObject *parent = nullptr)
        : RenderTask(parent)
    {
    }

    ~RenderSession() override
    {
        releaseResources();
    }

    QSet<ItemId> usedItems() const override
    {
        return (mImpl ? mImpl->usedItems() : QSet<ItemId>());
    }

    bool usesMouseState() const
    {
        return (mImpl && mImpl->usesMouseState());
    }

    bool usesKeyboardState() const 
    {
        return (mImpl && mImpl->usesKeyboardState());
    }

private:
    bool initialize(RenderAPI api) override
    {
        if (!mImpl)
            mImpl = RenderSessionBase::create(api);
        return static_cast<bool>(mImpl);
    }

    void prepare(bool itemsChanged,
        EvaluationType evaluationType) override
    {
        mImpl->prepare(itemsChanged, evaluationType);
    }

    void configure() override
    {
        mImpl->configure();
    }

    void configured() override
    {
        mImpl->configured();
    }

    void render() override
    {
        mImpl->render();
    }

    void finish() override  
    {
        mImpl->finish();
    }

    void release() override
    {
        mImpl.reset();
    }

    std::unique_ptr<RenderSessionBase> mImpl;
};
