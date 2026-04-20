#include "CustomActions.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "Singletons.h"
#include "ScriptEngine.h"
#include "editors/EditorManager.h"
#include "objects/AppScriptObject.h"
#include <QDirIterator>
#include <QApplication>

namespace {
    QString extractManifest(const QString &string)
    {
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

    QJSValue parseManifest(const QString &filePath, ScriptEngine &scriptEngine)
    {
        auto source = QString();
        if (Singletons::fileCache().getSource(filePath, &source))
            source = extractManifest(source);

        auto manifest = QJSValue();
        if (!source.isEmpty()) {
            scriptEngine.evaluateScript(source, filePath);
            manifest = scriptEngine.getGlobal("manifest");
            scriptEngine.setGlobal("manifest", QJSValue::UndefinedValue);
        }
        if (!manifest.isObject())
            manifest = scriptEngine.jsEngine().newObject();

        return manifest;
    }
} // namespace

CustomAction::CustomAction(const QString &filePath) : mFilePath(filePath)
{
    const auto fileInfo = QFileInfo(mFilePath);

    const auto dirName = fileInfo.dir().dirName();
    if (dirName != ActionsDir) {
        setObjectName(dirName);
        setText(dirName + "/" + fileInfo.baseName());
    } else {
        setObjectName(fileInfo.baseName());
        setText(fileInfo.baseName());
    }
}

void CustomAction::openInEditor()
{
    Singletons::editorManager().openEditor(mFilePath);
}

bool CustomAction::updateManifest(ScriptEngine &scriptEngine)
{
    auto manifest = parseManifest(mFilePath, scriptEngine);
    setEnabled(!manifest.isUndefined());
    if (manifest.isUndefined())
        return false;

    auto name = manifest.property("name");
    if (!name.isUndefined())
        setText(name.toString());

    auto applicable = manifest.property("applicable");
    if (!applicable.isUndefined()) {
        const auto result = (applicable.isCallable()
                ? scriptEngine.call(applicable, {}, 0)
                : applicable);
        setEnabled(result.isBool() && result.toBool());
    }
    return true;
}

MessagePtrSet CustomAction::apply(const QModelIndexList &selection)
{
    mScriptEngine.reset();

    mScriptEngine = ScriptEngine::make(objectName(), mFilePath);
    mScriptEngine->appScriptObject().setSelection(selection);

    applyInEngine(*mScriptEngine);

    mScriptEngine->appScriptObject().endBackgroundUpdate();

    return mScriptEngine->resetMessages();
}

void CustomAction::applyInEngine(ScriptEngine &scriptEngine) const
{
    auto source = QString();
    if (Singletons::fileCache().getSource(mFilePath, &source))
        scriptEngine.evaluateScript(source, mFilePath);
}

//-------------------------------------------------------------------------

CustomActions::CustomActions(QObject *parent) { }

CustomActions::~CustomActions() = default;

void CustomActions::actionTriggered()
{
    Q_ASSERT(onMainThread());
    auto &action = static_cast<CustomAction &>(
        *qobject_cast<QAction *>(QObject::sender()));

    const auto modifiers = QApplication::queryKeyboardModifiers();
    if (modifiers & Qt::ControlModifier)
        return action.openInEditor();

    mMessages = action.apply(mSelection);
}

void CustomActions::updateActions()
{
    Q_ASSERT(onMainThread());
    Singletons::fileCache().updateFromEditors();

    mMessages.clear();
    mActions.clear();

    for (const auto &dir : getApplicationDirectories(ActionsDir)) {
        auto scriptEngine = ScriptEngine::make(dir);
        scriptEngine->appScriptObject().setSelection(mSelection);

        auto it = QDirIterator(dir.path(), QStringList() << "*.js", QDir::Files,
            QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto filePath = toNativeCanonicalFilePath(it.next());
            auto action = std::make_shared<CustomAction>(filePath);
            if (!action->updateManifest(*scriptEngine))
                continue;

            connect(action.get(), &QAction::triggered, this,
                &CustomActions::actionTriggered);

            // keep only first action with identical id
            mActions.emplace(action->objectName(), std::move(action));
        }
        mMessages += scriptEngine->resetMessages();
    }
}

void CustomActions::setSelection(const QModelIndexList &selection)
{
    Q_ASSERT(onMainThread());
    mSelection = selection;
}

QList<CustomActionPtr> CustomActions::getApplicableActions()
{
    Q_ASSERT(onMainThread());
    updateActions();

    auto actions = QList<CustomActionPtr>();
    for (const auto &[name, action] : mActions)
        if (action->isEnabled())
            actions += action;
    return actions;
}

bool CustomActions::applyAction(const QString &id)
{
    Q_ASSERT(onMainThread());
    auto action = getActionById(id);
    if (!action)
        return false;

    mMessages += action->apply(mSelection);
    return true;
}

CustomActionPtr CustomActions::getActionById(const QString &id)
{
    for (const auto &dir : getApplicationDirectories(ActionsDir)) {
        auto it = QDirIterator(dir.path(), QStringList() << "*.js", QDir::Files,
            QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto filePath = toNativeCanonicalFilePath(it.next());
            auto action = std::make_shared<CustomAction>(filePath);
            if (action->objectName() == id)
                return action;
        }
    }
    return nullptr;
}

bool CustomActions::applyActionInEngine(const QString &id,
    ScriptEngine &scriptEngine)
{
    auto action = getActionById(id);
    if (!action)
        return false;
    action->applyInEngine(scriptEngine);
    return true;
}
