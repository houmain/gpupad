#pragma once

#include "Evaluation.h"
#include "Renderer.h"
#include "session/SessionModel.h"

class RenderSessionBase
{
public:
    static std::unique_ptr<RenderSessionBase> create(RenderAPI api);

    virtual ~RenderSessionBase() = default;
    virtual void prepare(bool itemsChanged, EvaluationType evaluationType) = 0;
    virtual void configure() = 0;
    virtual void configured() = 0;
    virtual void render() = 0;
    virtual void finish() = 0;
    virtual void release() = 0;
    virtual QSet<ItemId> usedItems() const = 0;
    virtual bool usesMouseState() const = 0;
    virtual bool usesKeyboardState() const = 0;
};
