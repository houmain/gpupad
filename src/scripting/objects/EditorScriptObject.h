#pragma once

#include <QObject>
#include <QSize>
#include <QJsonValue>

class AppScriptObject;

class EditorScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged);
    Q_PROPERTY(QString type READ type NOTIFY typeChanged);
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged);
    Q_PROPERTY(QJsonValue viewportSize READ viewportSize NOTIFY viewportResized)

public:
    EditorScriptObject(AppScriptObject *appScriptObject,
        const QString &fileName);
    ~EditorScriptObject();

    void resetAppScriptObject();
    void update();

    const QString &fileName() const { return mFileName; }
    const QString &type() const { return mEditorType; }
    const QString &title() const { return mTitle; }
    void setTitle(QString title);
    QJsonValue viewportSize() const;
    bool viewportSizeWasRead() const { return mViewportSizeWasRead; }

Q_SIGNALS:
    void fileNameChanged();
    void typeChanged();
    void titleChanged();
    void viewportResized();

private:
    AppScriptObject *mAppScriptObject{};
    QString mFileName;
    QString mTitle;
    QString mEditorType;
    QSize mViewportSize;
    mutable bool mViewportSizeWasRead{};
};
