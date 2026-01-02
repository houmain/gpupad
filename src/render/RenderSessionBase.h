#pragma once

#include "RenderTask.h"
#include "MessageList.h"
#include "TextureData.h"
#include "render/ShareSync.h"
#include "session/SessionModel.h"
#include "scripting/IScriptRenderSession.h"
#include "scripting/ScriptSession.h"
#include <map>
#include <QMap>
#include <QMutex>

class BufferBase;
class TextureBase;
class ShareSync;

struct UniformBinding
{
    ItemId bindingItemId;
    QString name;
    Binding::BindingType type;
    bool transpose;
    ScriptValueList values;
};

struct SamplerBinding
{
    ItemId bindingItemId;
    QString name;
    TextureBase *texture;
    Binding::Filter minFilter;
    Binding::Filter magFilter;
    bool anisotropic;
    Binding::WrapMode wrapModeX;
    Binding::WrapMode wrapModeY;
    Binding::WrapMode wrapModeZ;
    QColor borderColor;
    Binding::ComparisonFunc comparisonFunc;
};

struct ImageBinding
{
    ItemId bindingItemId;
    QString name;
    TextureBase *texture;
    int level;
    int layer;
    Binding::ImageFormat format;
};

struct BufferBinding
{
    ItemId bindingItemId;
    QString name;
    BufferBase *buffer;
    ItemId blockItemId;
    QString offset;
    QString rowCount;
    int stride;
};

struct SubroutineBinding
{
    ItemId bindingItemId;
    QString name;
    QString subroutine;
};

struct Bindings
{
    std::map<QString, UniformBinding> uniforms;
    std::map<QString, SamplerBinding> samplers;
    std::map<QString, ImageBinding> images;
    std::map<QString, BufferBinding> buffers;
    std::map<QString, SubroutineBinding> subroutines;
};

using BindingState = QStack<Bindings>;
using Command = std::function<void(BindingState &)>;

class RenderSessionBase : public RenderTask, public IScriptRenderSession
{
public:
    static std::unique_ptr<RenderSessionBase> create(RendererPtr renderer,
        const QString &basePath);

    RenderSessionBase(RendererPtr renderer, const QString &basePath,
        QObject *parent = nullptr);
    virtual ~RenderSessionBase();

    QThread *renderThread() override { return renderer().renderThread(); }
    const QString &basePath() override { return mBasePath; }
    void prepare(bool itemsChanged, EvaluationType evaluationType) override;
    void configure() override;
    void configured() override;
    void release() override;
    QSet<ItemId> usedItems() const override;
    SessionModel &sessionModelCopy() override { return mSessionModelCopy; }
    quint64 getTextureHandle(ItemId itemId) override { return 0; }
    quint64 getBufferHandle(ItemId itemId) override { return 0; }

    const Session &session() const;
    bool itemsChanged() const { return mItemsChanged; }
    EvaluationType evaluationType() const { return mEvaluationType; }
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    bool usesViewportSize(const QString &fileName) const;

    int getBufferSize(const Buffer &buffer);
    void evaluateBlockProperties(const Block &block, int *offset, int *rowCount,
        bool cached = true);
    void evaluateTextureProperties(const Texture &texture, int *width,
        int *height, int *depth, int *layers, bool cached = true);
    void evaluateTargetProperties(const Target &target, int *width, int *height,
        int *layers, bool cached = true);

protected:
    void addMessage(MessagePtr message) { mMessages += message; }
    void addUsedItems(const QSet<ItemId> &itemIds) { mUsedItems += itemIds; }
    bool updatingPreviewTextures() const;

    template <typename CommandQueue>
    void reuseUnmodifiedItems(CommandQueue &commandQueue,
        CommandQueue &prevCommandQueue) noexcept;

    template <typename RenderSession, typename CommandQueue>
    void buildCommandQueue(CommandQueue &commandQueue) noexcept;

    template <typename CommandQueue>
    void executeCommandQueue(CommandQueue &commandQueue) noexcept;

    template <typename CommandQueue>
    void beginDownloadModifiedResources(CommandQueue &commandQueue) noexcept;

    template <typename TimerQueries, typename ToNanoseconds>
    void outputTimerQueries(TimerQueries &timerQueries,
        const ToNanoseconds &toNanoseconds) noexcept;

    template <typename CommandQueue>
    void finishCommandQueue(CommandQueue &commandQueue, ShareSyncPtr shareSync) noexcept;

private:
    struct GroupIteration
    {
        size_t commandQueueBeginIndex;
        int iterations;
        int iterationsLeft;
    };

    void setNextCommandQueueIndex(size_t index);
    void invalidateCachedProperties();
    QList<int> getCachedProperties(ItemId itemId);
    void updateCachedProperties(ItemId itemId, QList<int> values);

    const QString mBasePath;
    QSet<ItemId> mUsedItems;
    bool mItemsChanged{};
    EvaluationType mEvaluationType{};
    SessionModel mSessionModelCopy;
    std::unique_ptr<ScriptSession> mScriptSession;
    MessagePtrSet mMessages;
    MessagePtrSet mPrevMessages;
    MessagePtrSet mTimerMessages;
    mutable QMutex mUsedItemsCopyMutex;
    QSet<ItemId> mUsedItemsCopy;
    mutable QMutex mPropertyCacheMutex;
    QMap<ItemId, QList<int>> mPropertyCache;
    size_t mNextCommandQueueIndex{};
    QMap<ItemId, GroupIteration> mGroupIterations;
    QMap<ItemId, ScriptValueList> mUniformBindingValues;
};

template <typename T, typename Item, typename... Args>
T *addOnce(std::map<ItemId, T> &list, const Item *item, Args &&...args)
{
    if (!item)
        return nullptr;
    auto it = list.find(item->id);
    if (it != list.end())
        return &it->second;
    return &list.emplace(std::piecewise_construct,
                    std::forward_as_tuple(item->id),
                    std::forward_as_tuple(*item, std::forward<Args>(args)...))
                .first->second;
}

template <typename C>
auto find(C &container, const QString &name)
{
    const auto it = container.find(name);
    return (it == container.end() ? nullptr : &it->second);
}
