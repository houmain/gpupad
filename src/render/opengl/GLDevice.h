#pragma once

#if defined(OPENGL_ENABLED)

#include "GLContext.h"
#include "MessageList.h"
#include "render/Device.h"
#include <QOffscreenSurface>
#include <QOpenGLDebugLogger>
#include <memory>

class GLDevice final : public Device
{
public:
    enum class Usage
    {
        Renderer,
        Window,
    };

    explicit GLDevice(Usage usage = Usage::Renderer, QObject *parent = nullptr);
    ~GLDevice() override;

    void moveToThread(QThread *thread) override;
    bool initialize(const AdapterIdentity &adapterIdentity) override;
    void shutdown() override;
    bool isValid() const override { return mInitialized; }

    GLContext &context();
    QOpenGLFunctions_4_5_Core &gl();

private:
    void createContext(QObject *parent = nullptr);
    void createRendererContext();
    bool initializeCurrentContext();
    void handleDebugMessage(const QOpenGLDebugMessage &message);

    Usage mUsage{};
    std::unique_ptr<GLContext> mContext;
    std::unique_ptr<QOffscreenSurface> mSurface;
    bool mInitialized{};
    MessagePtrSet mMessages;
    std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
};

QString getFirstGLError();

#endif // defined(OPENGL_ENABLED)
