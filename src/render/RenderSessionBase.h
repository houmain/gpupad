#pragma once

#include "BufferBase.h"
#include "RenderTask.h"
#include "MessageList.h"
#include "TextureData.h"
#include "ShareHandle.h"
#include "session/SessionModel.h"
#include "scripting/IScriptRenderSession.h"
#include "scripting/ScriptSession.h"
#include <map>
#include <functional>
#include <optional>
#include <utility>
#include <vector>
#include <QMap>
#include <QMutex>

class TextureBase;
class MediaFrameWriter;
class SoundOutput;

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

using Duration = std::chrono::duration<double>;
using BindingState = QStack<Bindings>;
using Command = std::function<void(BindingState &)>;
using SoundBufferKey = std::pair<ItemId, int>;
using AudioTextureKey = std::pair<ItemId, int>;

class RenderSessionBase : public RenderTask, public IScriptRenderSession
{
public:
    static std::unique_ptr<RenderSessionBase> create(RendererPtr renderer);

    RenderSessionBase(RendererPtr renderer, QObject *parent = nullptr);
    virtual ~RenderSessionBase();

    QThread *renderThread() override { return renderer().renderThread(); }
    void prepare(bool itemsChanged, EvaluationType evaluationType) override;
    void configure() override;
    void configured() override;
    void release() override;
    QSet<ItemId> usedItems() const override;
    SessionModel &sessionModelCopy() override { return mSessionModelCopy; }
    quint64 getTextureHandle(ItemId itemId) override { return 0; }
    quint64 getBufferHandle(ItemId itemId) override { return 0; }
    virtual std::vector<Duration> resetTimeQueries(size_t count) = 0;
    virtual std::shared_ptr<void> beginTimeQuery(size_t index) = 0;

    const Session &session() const;
    bool itemsChanged() const { return mItemsChanged; }
    EvaluationType evaluationType() const { return mEvaluationType; }
    bool usesMouseState() const;
    bool usesKeyboardState() const;
    bool usesViewportSize(const QString &fileName) const;
    void setBindingValues(ItemId bindingId, QStringList values);

    void setSoundPlaying(bool playing);
    std::optional<double> soundTime() const;
    std::optional<double> soundGenerationTime() const;
    void synchronizeSoundToAppTime();
    MediaFrameWriter &mediaFrameWriter() { return *mMediaFrameWriter; }

    int getBufferSize(const Buffer &buffer);
    void evaluateBlockProperties(const Block &block, int *offset, int *rowCount,
        bool cached = true);
    void evaluateTextureProperties(const Texture &texture, int *width,
        int *height, int *depth, int *layers, bool cached = true);
    void evaluateTargetProperties(const Target &target, int *width, int *height,
        int *layers, bool cached = true);

protected:
    static const auto maxTimeQueries = 128;

    void addMessage(MessagePtr message) { mMessages += message; }
    void addUsedItems(const QSet<ItemId> &itemIds) { mUsedItems += itemIds; }
    bool updatingTimerQueries() const;
    size_t timeQueryCount() const { return mTimeQueryCallIds.size(); }
    void obtainTimeQueryResults();

    template <typename CommandQueue>
    void reuseUnmodifiedItems(CommandQueue &commandQueue,
        CommandQueue &prevCommandQueue) noexcept;

    template <typename RenderSession, typename CommandQueue>
    void buildCommandQueue(CommandQueue &commandQueue) noexcept;

    template <typename CommandQueue>
    void executeCommandQueue(CommandQueue &commandQueue) noexcept;

    template <typename CommandQueue>
    void beginDownloadModifiedResources(CommandQueue &commandQueue) noexcept;

    template <typename CommandQueue>
    void finishCommandQueue(CommandQueue &commandQueue) noexcept;

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
    std::optional<size_t> addTimeQuery(ItemId callId);
    void evaluateBindingValues(const Binding &binding,
        ScriptEngine &scriptEngine);

    static Bindings mergeBindings(const BindingState &state);

    template <typename CommandQueue>
    void applyAudioTextures(CommandQueue &commandQueue, Bindings &bindings,
        int chunkIndex);
    template <typename CommandQueue>
    static BufferBase &getSoundBuffer(CommandQueue &commandQueue,
        ItemId callItemId, int chunkIndex, int size);
    template <typename CommandQueue>
    Bindings prepareSoundChunkBindings(CommandQueue &commandQueue,
        Bindings bindings, ItemId callItemId, int chunkIndex, int bufferSize);

    int getSoundBufferSize() const;
    int getSoundWorkGroupCount(int bufferSize) const;
    bool hasAudio() const;
    void prepareAudioGeneration();
    void prepareAudioTextureFrames();
    uint64_t advanceVisualAudioSampleBase();
    void prepareSoundBuffer(Bindings &bindings, ItemId callItemId,
        int chunkIndex, BufferBase &buffer) const;
    QByteArray getSoundBufferData(const BufferBase &buffer) const;
    void writeAudioBuffers(
        std::vector<std::pair<SoundBufferKey, QByteArray>> soundBuffers);
    bool recording() const;

    QSet<ItemId> mUsedItems;
    bool mItemsChanged{ };
    EvaluationType mEvaluationType{ };
    SessionModel mSessionModelCopy;
    std::unique_ptr<ScriptSession> mScriptSession;
    MessagePtrSet mMessages;
    MessagePtrSet mPrevMessages;
    MessagePtrSet mLastResetMessages;
    MessagePtrSet mTimeQueryMessages;
    std::vector<ItemId> mTimeQueryCallIds;
    mutable QMutex mUsedItemsCopyMutex;
    QSet<ItemId> mUsedItemsCopy;
    mutable QMutex mPropertyCacheMutex;
    QMap<ItemId, QList<int>> mPropertyCache;
    size_t mNextCommandQueueIndex{ };
    QMap<ItemId, GroupIteration> mGroupIterations;
    QMap<ItemId, ScriptValueList> mBindingValues;
    QMap<ItemId, QStringList> mBindingValueOverrides;
    std::unique_ptr<MediaFrameWriter> mMediaFrameWriter;
    std::unique_ptr<SoundOutput> mSoundOutput;
    std::vector<QMap<ItemId, TextureData>> mAudioTextureFrames;
    uint64_t mAudioSampleBase{ };
    uint64_t mVisualAudioSampleBase{ };
    int mAudioSampleRate{ };
    int mAudioChunkSize{ };
    int mAudioChunkCount{ };
    bool mVisualAudioSampleBaseInitialized{ };
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

template <typename C, typename T>
auto find(C &container, const T &key)
{
    const auto it = container.find(key);
    return (it == container.end() ? nullptr : &it->second);
}
