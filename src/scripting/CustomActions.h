#pragma once

#include "MessageList.h"
#include <QModelIndex>
#include <QAction>

class QAction;
using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
using CustomActionPtr = std::shared_ptr<class CustomAction>;

class CustomAction final : public QAction
{
public:
    explicit CustomAction(const QString &filePath);
    bool updateManifest(ScriptEngine &scriptEngine, MessagePtrSet &messages);
    void apply(const QModelIndexList &selection, MessagePtrSet &messages);
    void applyInEngine(ScriptEngine &scriptEngine,
        MessagePtrSet &messages) const;

private:
    const QString mFilePath;
    ScriptEnginePtr mScriptEngine;
};

class CustomActions final : public QObject
{
    Q_OBJECT
public:
    explicit CustomActions(QObject *parent = nullptr);
    ~CustomActions();

    // main thread
    void setSelection(const QModelIndexList &selection);
    QList<CustomActionPtr> getApplicableActions();

    // any thread
    static bool applyActionInEngine(const QString &id, ScriptEngine &scriptEngine,
        MessagePtrSet &messages);

private:
    void actionTriggered();
    void updateActions();
    static CustomActionPtr getActionById(const QString &id);

    std::map<QString, CustomActionPtr> mActions;
    MessagePtrSet mMessages;
    QModelIndexList mSelection;
};
