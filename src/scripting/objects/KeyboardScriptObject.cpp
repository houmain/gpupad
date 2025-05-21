#include "KeyboardScriptObject.h"
#include <QJsonArray>

KeyboardScriptObject::KeyboardScriptObject(QObject *parent)
    : QObject(parent) { }

void KeyboardScriptObject::update(const InputState &state)
{
    mWasRead = false;
    auto keys = QJsonArray();
    for (auto key : state.keyStates())
        keys.push_back(static_cast<int>(key));
    mKeys = keys;
    Q_EMIT changed();
}

const QJsonValue &KeyboardScriptObject::keys() const
{
    mWasRead = true;
    return mKeys;
}
