#ifndef GLITEM_H
#define GLITEM_H

#include "GLContext.h"
#include "GLObject.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "session/ItemFunctions.h"
#include "MessageList.h"
#include "FileDialog.h"
#include "FileCache.h"
#include <memory>
#include <QSet>

template<typename T>
inline T *checkVersion(T *gl, const char* name,
    ItemId itemId, MessagePtrSet &messages)
{
    if (gl)
        return gl;
    messages += Singletons::messageList().insert(itemId,
        MessageType::OpenGLVersionNotAvailable, name);
    return nullptr;
}

inline auto check(QOpenGLFunctions_4_0_Core *gl,
    ItemId itemId, MessagePtrSet &messages)
{
    return checkVersion(gl, "4.0", itemId, messages);
}

inline auto check(QOpenGLFunctions_4_2_Core *gl,
    ItemId itemId, MessagePtrSet &messages)
{
    return checkVersion(gl, "4.2", itemId, messages);
}

inline auto check(QOpenGLFunctions_4_3_Core *gl,
    ItemId itemId, MessagePtrSet &messages)
{
    return checkVersion(gl, "4.3", itemId, messages);
}

inline auto check(QOpenGLFunctions_4_5_Core *gl,
    ItemId itemId, MessagePtrSet &messages)
{
    return checkVersion(gl, "4.5", itemId, messages);
}

#endif // GLITEM_H
