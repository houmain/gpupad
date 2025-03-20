#include "CustomActions.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "Singletons.h"
#include "ScriptEngine.h"
#include "objects/AppScriptObject.h"
#include "objects/SessionScriptObject.h"
#include <QDirIterator>
#include <QFileInfo>

namespace {
    QString extractManifest(const QString &string)
    {
        // TODO: improve
        const auto begin = string.indexOf("manifest");
        if (begin < 0)
            return {};
        auto it = begin;
        const auto end = string.size();
        auto level = 0;
        for (; it < end; ++it) {
            const auto c = string.at(it);
            if (c == '{') {
                ++level;
            } else if (c == '}') {
                if (--level == 0)
                    return string.mid(begin, it - begin + 1);
            }
        }
        return {};
    }
} // namespace

class CustomAction final : public QAction
{
private:
    const QString mFilePath;
    ScriptEnginePtr mScriptEngine;

    QJSValue parseManifest(ScriptEngine &scriptEngine, MessagePtrSet &messages)
    {
        auto source = QString();
        if (Singletons::fileCache().getSource(mFilePath, &source))
            source = extractManifest(source);

        auto manifest = QJSValue();
        if (!source.isEmpty()) {
            scriptEngine.evaluateScript(source, mFilePath, messages);
            manifest = scriptEngine.getGlobal("manifest");
            scriptEngine.setGlobal("manifest", QJSValue::UndefinedValue);
        }
        if (!manifest.isObject())
            manifest = scriptEngine.jsEngine().newObject();

        return manifest;
    }

public:
    explicit CustomAction(const QString &filePath) : mFilePath(filePath)
    {
        const auto fileInfo = QFileInfo(mFilePath);
        setText(fileInfo.baseName());
        const auto dirName = fileInfo.dir().dirName();
        if (dirName != ActionsDir)
            setText(dirName + "/" + fileInfo.baseName());
    }

    bool updateManifest(ScriptEngine &scriptEngine, MessagePtrSet &messages)
    {
        auto manifest = parseManifest(scriptEngine, messages);
        setEnabled(!manifest.isUndefined());
        if (manifest.isUndefined())
            return false;

        auto name = manifest.property("name");
        if (!name.isUndefined())
            setText(name.toString());

        auto applicable = manifest.property("applicable");
        if (!applicable.isUndefined()) {
            const auto result = (applicable.isCallable()
                    ? scriptEngine.call(applicable, {}, 0, messages)
                    : applicable);
            setEnabled(result.isBool() && result.toBool());
        }
        return true;
    }

    void apply(const QModelIndexList &selection, MessagePtrSet &messages)
    {
        mScriptEngine.reset();

        const auto basePath = QFileInfo(mFilePath).absolutePath();
        mScriptEngine = ScriptEngine::make(basePath);
        mScriptEngine->appScriptObject().setSelection(selection);

        auto source = QString();
        if (Singletons::fileCache().getSource(mFilePath, &source))
            mScriptEngine->evaluateScript(source, mFilePath, messages);

        // TODO: run in cancelable background thread
        mScriptEngine->appScriptObject()
            .sessionScriptObject()
            .endBackgroundUpdate();
    }
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

    mMessages.clear();
    mActions.clear();

    for (const auto &dir : getApplicationDirectories(ActionsDir)) {
        auto scriptEngine = ScriptEngine::make(dir.path());

        auto it = QDirIterator(dir.path(), QStringList() << "*.js", QDir::Files,
            QDirIterator::Subdirectories);
        while (it.hasNext()) {
            // keep only last action with identical name
            const auto filePath = toNativeCanonicalFilePath(it.next());
            auto action = std::make_unique<CustomAction>(filePath);
            if (!action->updateManifest(*scriptEngine, mMessages))
                continue;

            connect(action.get(), &QAction::triggered, this,
                &CustomActions::actionTriggered);
            mActions[action->text()] = std::move(action);
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
        if (action->isEnabled())
            actions += action.get();
    return actions;
}
