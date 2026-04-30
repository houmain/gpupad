#pragma once

#include "MessageList.h"
#include "Evaluation.h"
#include "FileDialog.h"
#include <QJSValue>
#include <QModelIndex>
#include <QDir>

struct Item;
struct Shader;
class SessionModel;
class MouseScriptObject;
class KeyboardScriptObject;
class EditorScriptObject;
class ItemScriptObject;
class IScriptRenderSession;
using ScriptEnginePtr = std::shared_ptr<class ScriptEngine>;
using WeakScriptEnginePtr = std::weak_ptr<class ScriptEngine>;

class AppScriptObject final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString evaluation READ evaluation WRITE setEvaluation NOTIFY
            evaluationChanged)
    Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(double time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(double timeDelta READ timeDelta NOTIFY timeDeltaChanged)
    Q_PROPERTY(QJSValue date READ date CONSTANT)
    Q_PROPERTY(QJSValue session READ session CONSTANT)
    Q_PROPERTY(QJSValue mouse READ mouse CONSTANT)
    Q_PROPERTY(QJSValue keyboard READ keyboard CONSTANT)
    Q_PROPERTY(
        QJSValue currentEditor READ currentEditor NOTIFY currentEditorChanged)
    Q_PROPERTY(QVariantMap palette READ palette NOTIFY paletteChanged)

public:
    AppScriptObject(const ScriptEnginePtr &enginePtr, const QDir &basePath);
    ~AppScriptObject();

    void setSelection(const QModelIndexList &selectedIndices);
    void beginBackgroundUpdate(IScriptRenderSession *renderSession);
    void endBackgroundUpdate();

    QString evaluation() const;
    void setEvaluation(QString mode);
    int frame() const { return mFrame; }
    void setFrame(int index);
    double time() const { return mTime; }
    void setTime(double time);
    double timeDelta() const { return mTimeDelta; }
    QJSValue date();
    QJSValue session();
    QJSValue mouse() { return mMouseProperty; }
    QJSValue keyboard() { return mKeyboardProperty; }
    QJSValue currentEditor();
    QVariantMap palette() const;

    Q_INVOKABLE bool isUntitled(QString fileName);
    Q_INVOKABLE QString getFileTitle(QString fileName);
    Q_INVOKABLE QJSValue saveEditor(QString fileName);
    Q_INVOKABLE QJSValue openFileDialog(QString pattern = "");
    Q_INVOKABLE QJSValue saveFileDialog(QString pattern = "");
    Q_INVOKABLE QJSValue loadLibrary(QString fileName);
    Q_INVOKABLE QJSValue callAction(QString id);
    Q_INVOKABLE QJSValue callAction(QString id, QJSValue arguments);
    Q_INVOKABLE void evaluateScript(QString fileName);
    Q_INVOKABLE QJSValue enumerateFiles(QString pattern);
    Q_INVOKABLE QJSValue enumerateDirs(QString pattern);
    Q_INVOKABLE QJSValue writeTextFile(QString fileName, QString string);
    Q_INVOKABLE QJSValue writeBinaryFile(QString fileName, QByteArray binary);
    Q_INVOKABLE QJSValue readTextFile(QString fileName);

    // session
    Q_INVOKABLE void clearSession();
    Q_INVOKABLE QJSValue getParentItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue findItem(QJSValue itemIdent, QJSValue originIdent,
        bool searchSubItems = false);
    Q_INVOKABLE QJSValue findItems(QJSValue itemIdent, QJSValue originIdent,
        bool searchSubItems = false);
    Q_INVOKABLE QJSValue findItems(QJSValue itemIdent);
    Q_INVOKABLE QJSValue trackItems(QJSValue itemIdent, QJSValue originIdent,
        bool searchSubItems, QJSValue callback);
    Q_INVOKABLE QJSValue trackItems(QJSValue itemIdent, QJSValue originIdent,
        QJSValue callback);
    Q_INVOKABLE QJSValue trackItems(QJSValue itemIdent, QJSValue callback);
    Q_INVOKABLE QJSValue insertItem(QJSValue object);
    Q_INVOKABLE QJSValue insertItem(QJSValue parentIdent, QJSValue object);
    Q_INVOKABLE QJSValue insertBeforeItem(QJSValue siblingIdent,
        QJSValue object);
    Q_INVOKABLE QJSValue insertAfterItem(QJSValue siblingIdent,
        QJSValue object);
    Q_INVOKABLE void replaceItems(QJSValue parentIdent, QJSValue array);
    Q_INVOKABLE void clearItems(QJSValue parentIdent);
    Q_INVOKABLE void deleteItem(QJSValue itemIdent);
    Q_INVOKABLE QJSValue openEditor(QJSValue fileNameOrItemIdent);
    Q_INVOKABLE void setBufferData(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setBlockData(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setTextureData(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setScriptSource(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE void setShaderSource(QJSValue itemIdent, QJSValue data);
    Q_INVOKABLE quint64 getTextureHandle(QJSValue itemIdent);
    Q_INVOKABLE quint64 getBufferHandle(QJSValue itemIdent);
    Q_INVOKABLE QJSValue processShader(QJSValue fileNameOrItemIdent,
        QString processType);

    const QDir &basePath() const { return mBasePath; }
    void deregisterEditorScriptObject(EditorScriptObject *object);
    void update();
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    bool usesViewportSize(const QString &fileName) const;

Q_SIGNALS:
    void evaluationChanged();
    void frameChanged();
    void timeChanged();
    void timeDeltaChanged();
    void currentEditorChanged();
    void paletteChanged();

private:
    friend class ItemScriptObject;
    using UpdateSessionFunction = std::function<void(SessionModel &)>;

    struct ItemObject
    {
        ItemScriptObject *object;
        QJSValue jsValue;
    };

    struct ItemTracking
    {
        QJSValue itemIdent;
        QJSValue callback;
        ItemId originId;
        bool searchSubItems;
        QSet<ItemId> addedItemIds;
    };

    QJSEngine &jsEngine();
    void throwJsError(const QString &message);
    void handleEvaluationModeChanged(EvaluationMode evaluationMode);
    void handleFrameChanged(int frame);
    void handleTimeChanged(double time);
    QString getAbsolutePath(const QString &fileName) const;
    void dispatchToMainThread(const std::function<void()> &function);
    QJSValue enumerate(QString pattern, bool directories);
    QJSValue tryGetEditorObject(const QString &fileName);
    QJSValue getEditorObject(const QString &fileName);
    QJSValue openFileDialog(QString pattern, FileDialog::Options options);

    // session
    bool isSessionAvailable() const;
    SessionModel &threadSessionModel();
    QJsonObject toJsonObject(const QJSValue &object);
    void withSessionModel(UpdateSessionFunction &&updateFunction);
    QModelIndex getOriginIndex(QJSValue originIdent);
    const Item *findSessionItem(QJSValue itemIdent,
        const QModelIndex &originIndex, bool searchSubItems);
    const Item *findSessionItem(QJSValue itemIdent);
    QList<const Item *> findSessionItems(QJSValue itemIdent,
        const QModelIndex &originIndex, bool searchSubItems);
    QJSValue makeItemObject(ItemId itemId);
    QJSValue makeItemArray(const QList<ItemId> &itemIds);
    QJSValue makeItemArray(const QList<const Item *> &items);
    QJSValue makeItemArray(const QList<Item *> &items);
    void handleItemAdded(const Item *item);
    void handleItemModified(const Item *item);
    void handleItemRemoved(const Item *item);
    bool itemMatchesFilter(const Item *item, const ItemTracking &tracking);
    void updateItemTracking(const Item *item, bool removed);
    void updateItemProperties(const Item *item);
    QJSValue insertItemAt(const Item *parent, int row, QJSValue object);
    QVariant processShader(const QString &fileName, SourceType sourceType,
        QString processType);

    template <typename... Args>
    QJSValue callFunction(const QJSValue &function, Args &&...args)
    {
        Q_ASSERT(function.isCallable());
        auto result = function.call({ std::forward<Args>(args)... });
        if (result.isError()) {
            throwJsError(result.toString());
            return QJSValue::UndefinedValue;
        }
        return result;
    }

    template <typename T>
    const T *findSessionItem(const QJSValue &itemObject)
    {
        return castItem<T>(findSessionItem(itemObject));
    }

    WeakScriptEnginePtr mEnginePtr;
    QJSEngine *mJsEngine{};
    QDir mBasePath;
    MouseScriptObject *mMouseScriptObject{};
    KeyboardScriptObject *mKeyboardScriptObject{};
    std::map<EditorScriptObject *, QJSValue> mEditorScriptObjects;

    QJSValue mMouseProperty;
    QJSValue mKeyboardProperty;
    QJSValue mDateProperty;
    QMap<QString, QJSValue> mLoadedLibraries;
    QObject *mMainThreadObject{};
    EvaluationMode mEvaluationMode{};
    int mFrame{};
    double mTime{};
    double mTimeDelta{};

    // session
    QJSValue mSelectionProperty;
    QJSValue mSessionProperty;
    IScriptRenderSession *mRenderSession{};
    std::vector<UpdateSessionFunction> mPendingSessionUpdates;
    std::map<ItemId, ItemObject> mItemObjects;
    std::map<int, ItemTracking> mItemTrackings;
    int mNextItemTrackingIndex{};
};
