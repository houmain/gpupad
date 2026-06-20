#pragma once

#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVertexArrayObject>
#include "GLRenderSession.h"
#include "GLObject.h"
#include "FileCache.h"
#include "FileDialog.h"
#include "MessageList.h"
#include "Singletons.h"
#include <memory>

class GLContext final : public QOpenGLContext, public QOpenGLFunctions_4_5_Core
{
    Q_OBJECT
public:
    using QOpenGLContext::QOpenGLContext;
    ~GLContext();

    bool initialize();
    void release();
    QOpenGLVertexArrayObject::Binder bindVertexArrayObject();
    QString getLastGLError();

private:
    void handleDebugMessage(const QOpenGLDebugMessage &message);

    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
    QOpenGLVertexArrayObject mVertexArrayObject;
    QString mLastGLError;
};
