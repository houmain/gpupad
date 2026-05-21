#pragma once

#include "session/ItemEnums.h"
#include "AdapterIdentity.h"
#include <QtGlobal>

class QThread;

class Device
{
public:
    using Type = ItemEnums::Renderer;

    explicit Device(Type type) : mType(type) { }
    virtual ~Device() = default;

    Type type() const { return mType; }

    virtual void moveToThread(QThread *thread) { Q_UNUSED(thread); }
    virtual bool initialize(const AdapterIdentity &adapter) = 0;
    virtual void shutdown() = 0;
    virtual bool isValid() const = 0;

private:
    const Type mType;
};
