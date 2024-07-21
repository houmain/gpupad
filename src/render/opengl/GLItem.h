#pragma once

#include "FileCache.h"
#include "FileDialog.h"
#include "GLContext.h"
#include "GLObject.h"
#include "MessageList.h"
#include "Singletons.h"
#include "session/Item.h"
#include "session/SessionModel.h"
#include <QSet>
#include <memory>

template <typename T>
inline T *checkVersion(T *gl, const char *name, ItemId itemId,
    MessagePtrSet &messages)
{
    if (gl)
        return gl;
    messages += MessageList::insert(itemId,
        MessageType::OpenGLVersionNotAvailable, name);
    return nullptr;
}

inline auto check(QOpenGLFunctions_4_0_Core *gl, ItemId itemId,
    MessagePtrSet &messages)
{
    return checkVersion(gl, "4.0", itemId, messages);
}

inline auto check(QOpenGLFunctions_4_2_Core *gl, ItemId itemId,
    MessagePtrSet &messages)
{
    return checkVersion(gl, "4.2", itemId, messages);
}

inline auto check(QOpenGLFunctions_4_3_Core *gl, ItemId itemId,
    MessagePtrSet &messages)
{
    return checkVersion(gl, "4.3", itemId, messages);
}

inline auto check(QOpenGLFunctions_4_5_Core *gl, ItemId itemId,
    MessagePtrSet &messages)
{
    return checkVersion(gl, "4.5", itemId, messages);
}
