#pragma once

#include <QOpenGLContext>
#include <QOpenGLFunctions_4_5_Core>
#include "GLRenderSession.h"
#include "GLObject.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "MessageList.h"
#include "Singletons.h"

class GLContext final : public QOpenGLContext, public QOpenGLFunctions_4_5_Core
{
    Q_OBJECT
public:
    static GLContext &currentContext()
    {
        auto current = QOpenGLContext::currentContext();
        Q_ASSERT(current);
        return *static_cast<GLContext *>(current);
    }

    using QOpenGLContext::QOpenGLContext;
};
