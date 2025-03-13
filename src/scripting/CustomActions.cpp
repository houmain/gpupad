#include "CustomActions.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "Singletons.h"
#include "ScriptEngine.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include <QDirIterator>
#include <QFileInfo>

class CustomAction final : public QAction
{
public:
    explicit CustomAction(const QString &filePath) : mFilePath(filePath)
    {
        setText(QFileInfo(mFilePath).baseName());

        auto source = QString();
        if (Singletons::fileCache().getSource(mFilePath, &source)) {
            const auto basePath = QFileInfo(mFilePath).absolutePath();
            mScriptEngine = ScriptEngine::make(basePath);
            mScriptEngine->evaluateScript(source, mFilePath, mMessages);
            auto name = mScriptEngine->getGlobal("name");
            if (!name.isUndefined())
                setText(name.toString());
        }
    }

    bool isApplicable(const QModelIndexList &selection, MessagePtrSet &messages)
    {
        if (!mScriptEngine)
            return false;

        auto applicable = mScriptEngine->getGlobal("applicable");
        if (applicable.isCallable()) {
            auto result = mScriptEngine->call(applicable,
                { getItems(selection) }, 0, messages);
            if (result.isBool())
                return result.toBool();
            return false;
        }
        return true;
    }

    void apply(const QModelIndexList &selection, MessagePtrSet &messages)
    {
        if (!mScriptEngine)
            return;

        auto apply = mScriptEngine->getGlobal("apply");
        if (apply.isCallable()) {
            mScriptEngine->call(apply, { getItems(selection) }, 0, messages);
        }
    }

private:
    QJSValue getItems(const QModelIndexList &selectedIndices)
    {
        auto array = mScriptEngine->jsEngine().newArray(selectedIndices.size());
        auto i = 0;
        for (const auto &index : selectedIndices)
            array.setProperty(i++,
                mScriptEngine->appScriptObject().sessionScriptObject().getItem(
                    index));
        return array;
    }

    const QString mFilePath;
    MessagePtrSet mMessages;
    ScriptEnginePtr mScriptEngine;
};

//-------------------------------------------------------------------------

CustomActions::CustomActions(QObject *parent) { }

CustomActions::~CustomActions() = default;

void CustomActions::actionTriggered()
{
    auto &action = static_cast<CustomAction &>(
        *qobject_cast<QAction *>(QObject::sender()));

    mMessages.clear();
    action.apply(mSelection, mMessages);
}

void CustomActions::updateActions()
{
    Singletons::fileCache().updateFromEditors();

    mActions.clear();

    for (const auto &dir : std::initializer_list<QDir>{
             getInstallDirectory("actions"), getUserDirectory("actions") }) {
        auto it = QDirIterator(dir.path(), QStringList() << "*.js", QDir::Files,
            QDirIterator::Subdirectories);
        while (it.hasNext()) {
            // keep only last action with identical name
            const auto filePath = toNativeCanonicalFilePath(it.next());
            auto action = new CustomAction(filePath);
            mActions[action->text()].reset(action);

            connect(action, &QAction::triggered, this,
                &CustomActions::actionTriggered);
        }
    }
}

void CustomActions::setSelection(const QModelIndexList &selection)
{
    mSelection = selection;
}

QList<QAction *> CustomActions::getApplicableActions()
{
    updateActions();

    auto actions = QList<QAction *>();
    for (const auto &[name, action] : mActions)
        if (action->isApplicable(mSelection, mMessages))
            actions += action.get();
    return actions;
}