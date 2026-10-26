#pragma once
#if defined(OPENGL_ENABLED)

#  include "GLContext.h"
#  include "message/MessageList.h"
#  include "render/Device.h"
#  include <QOffscreenSurface>

class GLDevice final : public Device
{
public:
    GLDevice();
    ~GLDevice() override;

    bool initialize() override;

    QOpenGLContext &context() { return mContext; }
    GLContext &gl() { return mGL; }

private:
    QOpenGLContext mContext;
    QOffscreenSurface *mSurface;
    GLContext mGL;
    MessageList mMessages;
};

#endif // defined(OPENGL_ENABLED)
