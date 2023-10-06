#pragma once

#include "../RenderSessionBase.h"
#include "Evaluation.h"
#include "MessageList.h"
#include "TextureData.h"
#include "session/SessionModel.h"
#include <QMutex>
#include <QMap>
#include <memory>

class VKRenderer;
class ScriptSession;
class QOpenGLTimerQuery;

class VKRenderSession final : public RenderSessionBase
{
public:
    explicit VKRenderSession(VKRenderer &renderer);
    ~VKRenderSession();

    void prepare(bool itemsChanged, EvaluationType evaluationType) override;
    void configure() override;
    void configured() override;
    void render() override;
    void finish() override;
    void release() override;

    QSet<ItemId> usedItems() const override;
    bool usesMouseState() const override;
    bool usesKeyboardState() const override;

private:
    VKRenderer &mRenderer;
};
