#pragma once

#include "session/Item.h"

class IScriptRenderSession
{
public:
    virtual ~IScriptRenderSession() = default;
    virtual quint64 getTextureHandle(ItemId itemId) = 0;
};
