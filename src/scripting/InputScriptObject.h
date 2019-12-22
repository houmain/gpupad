#ifndef INPUTSCRIPTOBJECT_H
#define INPUTSCRIPTOBJECT_H

#include <QObject>
#include <QJsonValue>
#include <QPointF>

class InputScriptObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue mouseX READ mouseX)
    Q_PROPERTY(QJsonValue mouseY READ mouseY)
public:
    explicit InputScriptObject(QObject *parent = nullptr);

    void setMousePosition(QPointF pos);
    QJsonValue mouseX() const;
    QJsonValue mouseY() const;

private:
    QPointF mMousePosition{ };
};

#endif // INPUTSCRIPTOBJECT_H
