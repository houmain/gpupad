#pragma once

#include <QObject>
#include "session/ItemEnums.h"

class Device : public QObject
{
    Q_OBJECT
public:
    using Type = ItemEnums::Renderer;

    explicit Device(Type type) : mType(type) { }
    virtual ~Device() = default;

    Type type() const { return mType; }

    virtual bool initialize() = 0;

private:
    const Type mType;
};
