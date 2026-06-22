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

class GLContext final : public QObject, public QOpenGLFunctions_4_5_Core
{
    Q_OBJECT
public:
    explicit GLContext(QObject *parent = nullptr);
    ~GLContext();

    bool initialize(QOpenGLContext *context);
    bool initialized() const { return static_cast<bool>(mContext); }
    QOpenGLVertexArrayObject::Binder bindVertexArrayObject();
    QString getLastGLError();
    bool hasExtension(const char *name) { return mContext->hasExtension(name); }

    template <typename T>
    T getProcAddress(const char *name)
    {
        return reinterpret_cast<T>(mContext->getProcAddress(name));
    }

private:
    void handleDebugMessage(const QOpenGLDebugMessage &message);

    QOpenGLContext *mContext{};
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
    std::unique_ptr<QOpenGLVertexArrayObject> mVertexArrayObject;
    QString mLastGLError;
};
