#ifndef GLITEM_H
#define GLITEM_H

#include "GLContext.h"
#include "GLObject.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "session/Item.h"
#include "MessageList.h"
#include "FileCache.h"
#include <memory>
#include <QSet>

template<typename T>
inline T *checkVersion(T *gl, const char* name, ItemId itemId)
{
    if (gl)
        return gl;
    Singletons::messageList().insert(itemId,
        MessageType::OpenGLVersionNotAvailable, name);
    return nullptr;
}

inline auto check(QOpenGLFunctions_4_0_Core *gl, ItemId itemId)
{
    return checkVersion(gl, "4.0", itemId);
}

inline auto check(QOpenGLFunctions_4_2_Core *gl, ItemId itemId)
{
    return checkVersion(gl, "4.2", itemId);
}

inline auto check(QOpenGLFunctions_4_3_Core *gl, ItemId itemId)
{
    return checkVersion(gl, "4.3", itemId);
}

inline auto check(QOpenGLFunctions_4_5_Core *gl, ItemId itemId)
{
    return checkVersion(gl, "4.5", itemId);
}

#endif // GLITEM_H
