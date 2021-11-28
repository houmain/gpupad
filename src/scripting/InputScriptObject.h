#ifndef INPUTSCRIPTOBJECT_H
#define INPUTSCRIPTOBJECT_H

#include <QObject>
#include <QJsonValue>
#include "InputState.h"

class InputScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue mouseCoord READ mouseCoord)
    Q_PROPERTY(QJsonValue mouseFragCoord READ mouseFragCoord)
    Q_PROPERTY(QJsonValue prevMouseCoord READ prevMouseCoord)
    Q_PROPERTY(QJsonValue prevMouseFragCoord READ prevMouseFragCoord)
    Q_PROPERTY(QJsonValue mousePressed READ mousePressed)
public:
    explicit InputScriptObject(QObject *parent = nullptr);

    void update(const InputState &inputState);
    
    QJsonValue mouseFragCoord() const;
    QJsonValue mouseCoord() const;
    QJsonValue prevMouseFragCoord() const;
    QJsonValue prevMouseCoord() const;
    QJsonValue mousePressed() const;
    bool wasRead() const { return mWasRead; }

private:
    InputState mInputState{ };
    mutable bool mWasRead{ };
};

#endif // INPUTSCRIPTOBJECT_H
