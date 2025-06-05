#pragma once

#include <QObject>
#include <QSize>
#include <QJsonValue>

class AppScriptObject;

class EditorScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue viewportSize READ viewportSize NOTIFY viewportResized)

public:
    EditorScriptObject(AppScriptObject *appScriptObject,
        const QString &fileName);
    ~EditorScriptObject();

    void resetAppScriptObject();
    void update();

    const QString &fileName() const { return mFileName; }
    QJsonValue viewportSize();

Q_SIGNALS:
    void viewportResized();

private:
    AppScriptObject *mAppScriptObject{ };
    QString mFileName;
    QSize mViewportSize{ 256, 256 };
};
