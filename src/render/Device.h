#pragma once

#include <QObject>
#include "session/ItemEnums.h"
#include "AdapterIdentity.h"

class QThread;

class Device : public QObject
{
    Q_OBJECT
public:
    using Type = ItemEnums::Renderer;

    explicit Device(Type type) : mType(type) { }
    virtual ~Device() = default;

    Type type() const { return mType; }

    virtual bool initialize(const AdapterIdentity &adapter) = 0;

private:
    const Type mType;
};
