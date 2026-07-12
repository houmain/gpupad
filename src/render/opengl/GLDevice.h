#pragma once
#if defined(OPENGL_ENABLED)

#  include "GLContext.h"
#  include "MessageList.h"
#  include "render/Device.h"
#  include <QOffscreenSurface>

class GLDevice final : public Device
{
public:
    explicit GLDevice(QObject *parent = nullptr);
    ~GLDevice() override;

    bool initialize(const AdapterIdentity &adapterIdentity) override;

    QOpenGLContext &context() { return mContext; }
    GLContext &gl() { return mGL; }

private:
    QOpenGLContext mContext;
    QOffscreenSurface mSurface;
    GLContext mGL;
    MessagePtrSet mMessages;
};

#endif // defined(OPENGL_ENABLED)
