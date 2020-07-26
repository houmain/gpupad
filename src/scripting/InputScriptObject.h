#ifndef INPUTSCRIPTOBJECT_H
#define INPUTSCRIPTOBJECT_H

#include <QObject>
#include <QJsonValue>
#include <QPointF>

class InputScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue mouseFragCoord READ mouseFragCoord)
public:
    explicit InputScriptObject(QObject *parent = nullptr);

    void setMouseFragCoord(QPointF pos);
    QJsonValue mouseFragCoord() const;

private:
    QPointF mMouseFragCoord{ };
};

#endif // INPUTSCRIPTOBJECT_H
