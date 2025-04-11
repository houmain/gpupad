#pragma once

#include "session/Item.h"

class SessionModel;

class IScriptRenderSession
{
public:
    virtual ~IScriptRenderSession() = default;
    virtual QThread *renderThread() = 0;
    virtual const QString &basePath() = 0;
    virtual SessionModel &sessionModelCopy() = 0;
    virtual quint64 getTextureHandle(ItemId itemId) = 0;
};
