#pragma once

#include "session/Item.h"

class SessionModel;

class IScriptRenderSession
{
public:
    virtual ~IScriptRenderSession() = default;
    virtual quint64 getTextureHandle(ItemId itemId) = 0;
    virtual SessionModel &sessionModelCopy() = 0;
};
