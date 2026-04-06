#pragma once

#include <QObject>
#include <QSize>
#include <QJsonValue>

class AppScriptObject;

class EditorScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName CONSTANT);
    Q_PROPERTY(QString type READ type CONSTANT);
    Q_PROPERTY(QJsonValue viewportSize READ viewportSize NOTIFY viewportResized)

public:
    EditorScriptObject(AppScriptObject *appScriptObject,
        const QString &fileName);
    ~EditorScriptObject();

    void resetAppScriptObject();
    void update();

    const QString &fileName() const { return mFileName; }
    const QString &type() const { return mEditorType; }
    QJsonValue viewportSize() const;
    bool viewportSizeWasRead() const { return mViewportSizeWasRead; }

Q_SIGNALS:
    void viewportResized();

private:
    AppScriptObject *mAppScriptObject{};
    QString mFileName;
    QString mEditorType;
    QSize mViewportSize;
    mutable bool mViewportSizeWasRead{};
};
