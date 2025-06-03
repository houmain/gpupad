#pragma once

#include <QObject>
#include <QJsonValue>

class EditorScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue viewportSize READ viewportSize NOTIFY viewportResized)

public:
    explicit EditorScriptObject(const QString &fileName,
        QObject *parent = nullptr);

    QJsonValue viewportSize() const;

Q_SIGNALS:
    void viewportResized();

private:
    QString mFileName;
};
