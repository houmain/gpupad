#pragma once

#if defined(OPENGL_ENABLED)

#  include "GLContext.h"
#  include "MessageList.h"
#  include "render/Device.h"
#  include <QOffscreenSurface>
#  include <memory>

class GLDevice final : public Device
{
public:
    explicit GLDevice(QObject *parent = nullptr);
    ~GLDevice() override;

    void moveToThread(QThread *thread) override;
    bool initialize(const AdapterIdentity &adapterIdentity) override;
    void shutdown() override;
    bool isValid() const override { return mInitialized; }

    GLContext &context();

private:
    void createContext(QObject *parent = nullptr);
    void createRendererContext();

    std::unique_ptr<GLContext> mContext;
    std::unique_ptr<QOffscreenSurface> mSurface;
    bool mInitialized{};
    MessagePtrSet mMessages;
};

#endif // defined(OPENGL_ENABLED)
