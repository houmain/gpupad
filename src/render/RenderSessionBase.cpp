
#include "RenderSessionBase.h"
#include "BufferBase.h"
#include "FileCache.h"
#include "InputState.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "media/MediaManager.h"
#include "scripting/ScriptEngine.h"
#include "scripting/ScriptSession.h"
#include "session/SessionModel.h"
#include "opengl/GLRenderSession.h"
#include "vulkan/VKRenderSession.h"
#include "direct3d/D3DRenderSession.h"
#include "media/MediaFrameWriter.h"
#include "media/SoundOutput.h"
#include <QStack>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {
    struct alignas(16) SoundBufferHeader
    {
        uint32_t sampleBaseLow;
        uint32_t sampleBaseHigh;
        uint32_t sampleRate;
        uint32_t frameCount;
    };

    static_assert(sizeof(SoundBufferHeader) == 16);

    inline constexpr auto SoundWorkGroupSize = 256;
    inline constexpr auto SoundBufferBindingName = "soundBuffer";
} // namespace

std::unique_ptr<RenderSessionBase> RenderSessionBase::create(
    RendererPtr renderer)
{
    auto session = std::unique_ptr<RenderSessionBase>();
    switch (renderer->type()) {
    case Renderer::Type::OpenGL:
#if defined(OPENGL_ENABLED)
        return std::make_unique<GLRenderSession>(renderer);
#endif
        break;

    case Renderer::Type::Vulkan:
#if defined(VULKAN_ENABLED)
        return std::make_unique<VKRenderSession>(renderer);
#endif
        break;

    case Renderer::Type::Direct3D:
#if defined(D3D_ENABLED)
        return std::make_unique<D3DRenderSession>(renderer);
#endif
        break;
    }
    return { };
}

RenderSessionBase::RenderSessionBase(RendererPtr renderer, QObject *parent)
    : RenderTask(std::move(renderer), parent)
    , mMediaFrameWriter(std::make_unique<MediaFrameWriter>())
    , mSoundOutput(std::make_unique<SoundOutput>())
{
    connect(mMediaFrameWriter.get(), &MediaFrameWriter::frameReady,
        mSoundOutput.get(), &SoundOutput::writeFrame);
}

RenderSessionBase::~RenderSessionBase() = default;

void RenderSessionBase::prepare(bool itemsChanged,
    EvaluationType evaluationType)
{
    Q_ASSERT(onMainThread());
    mItemsChanged = itemsChanged;
    mEvaluationType = evaluationType;
    mPrevMessages = std::exchange(mMessages, { });

    if (itemsChanged)
        invalidateCachedProperties();

    if (mScriptSession) {
        mScriptSession->update();
    } else {
        mEvaluationType = EvaluationType::Reset;
    }
    const auto sessionReset =
        (mItemsChanged || mEvaluationType == EvaluationType::Reset);
    if (sessionReset) {
        mUsedItems.clear();
        mSessionModelCopy = Singletons::sessionModel();
        mBindingValueOverrides.clear();
    }
    if (!recording()) {
        mSoundOutput->setBufferDuration(session().audioBufferSize);
        mSoundOutput->setFormat(session().audioSampleRate,
            session().audioChunkSize);
        mSoundOutput->setAppTime(Singletons::inputState().time(),
            mEvaluationType == EvaluationType::Reset);
    }
    prepareAudioGeneration();
}

void RenderSessionBase::setBindingValues(ItemId bindingId, QStringList values)
{
    Q_ASSERT(onMainThread());
    mBindingValueOverrides[bindingId] = std::move(values);
}

void RenderSessionBase::setSoundPlaying(bool playing)
{
    Q_ASSERT(onMainThread());
    mSoundOutput->setPlaying(playing);
}

std::optional<double> RenderSessionBase::soundTime() const
{
    Q_ASSERT(onMainThread());
    return mSoundOutput->playbackTime();
}

std::optional<double> RenderSessionBase::soundGenerationTime() const
{
    Q_ASSERT(onMainThread());
    const auto frameCount = mMediaFrameWriter->audioFrameCount();
    if (updating() && mAudioSampleRate > 0 && frameCount > 0) {
        const auto sampleBase = mAudioSampleBase
            + static_cast<uint64_t>(frameCount);
        return static_cast<double>(sampleBase) / mAudioSampleRate;
    }
    return mSoundOutput->generationTime();
}

void RenderSessionBase::synchronizeSoundToAppTime()
{
    Q_ASSERT(onMainThread());
    mSoundOutput->synchronizeToAppTime();
    mVisualAudioSampleBaseInitialized = false;
}

bool RenderSessionBase::recording() const
{
    return mMediaFrameWriter->recording();
}

int RenderSessionBase::getSoundBufferSize() const
{
    return static_cast<int>(sizeof(SoundBufferHeader))
        + mAudioChunkSize * BytesPerFrame;
}

int RenderSessionBase::getSoundWorkGroupCount(int bufferSize) const
{
    const auto sampleDataSize =
        static_cast<int>(bufferSize - sizeof(SoundBufferHeader));
    Q_ASSERT(sampleDataSize >= 0 && sampleDataSize % BytesPerFrame == 0);
    const auto frameCount = sampleDataSize / BytesPerFrame;
    return (frameCount + SoundWorkGroupSize - 1) / SoundWorkGroupSize;
}

bool RenderSessionBase::hasAudio() const
{
    auto result = Singletons::mediaManager().hasAudio();
    mSessionModelCopy.forEachItem<Call>([&](const Call &call) {
        result |= call.checked && call.callType == Call::CallType::ComputeSound;
    });
    return result;
}

void RenderSessionBase::prepareAudioGeneration()
{
    if (mAudioSampleRate != session().audioSampleRate
        || mAudioChunkSize != session().audioChunkSize)
        mVisualAudioSampleBaseInitialized = false;
    mAudioSampleRate = session().audioSampleRate;
    mAudioChunkSize = session().audioChunkSize;

    if (!recording())
        mMediaFrameWriter->beginFrame({
            .audioSampleBase = mSoundOutput->sampleBase(),
            .audioFrameCount = mSoundOutput->requestedChunkCount(hasAudio())
                * mAudioChunkSize,
            .audioSampleRate = mAudioSampleRate,
        });

    mAudioSampleBase = mMediaFrameWriter->audioSampleBase();
    mAudioChunkCount =
        (mMediaFrameWriter->audioFrameCount() + mAudioChunkSize - 1)
        / mAudioChunkSize;
    prepareAudioTextureFrames();
}

void RenderSessionBase::prepareAudioTextureFrames()
{
    mAudioTextureFrames.clear();
    mAudioTextureFrames.resize(static_cast<size_t>(mAudioChunkCount));
    auto &mediaManager = Singletons::mediaManager();
    for (auto chunkIndex = 0; chunkIndex < mAudioChunkCount; ++chunkIndex) {
        mediaManager.prepareAudioFrame(mAudioSampleBase
            + static_cast<uint64_t>(chunkIndex) * mAudioChunkSize);
        mSessionModelCopy.forEachItem<Texture>([&](const Texture &texture) {
            if (!isAudioSource(texture.sourceType))
                return;

            auto width = 0, height = 0, depth = 0, layers = 0;
            evaluateTextureProperties(texture, &width, &height, &depth,
                &layers);
            auto data = TextureData{ };
            const auto source = MediaSource{ texture.fileName,
                texture.sourceType, texture.target, QSize(width, 1) };
            if (Singletons::fileCache().getTexture(source, &data))
                mAudioTextureFrames[chunkIndex].insert(texture.id,
                    std::move(data));
        });
    }

    mediaManager.prepareAudioFrame(advanceVisualAudioSampleBase());
}

uint64_t RenderSessionBase::advanceVisualAudioSampleBase()
{
    const auto time = Singletons::inputState().time();
    Q_ASSERT(std::isfinite(time) && time >= 0.0);
    const auto targetSampleBase =
        static_cast<uint64_t>(time * std::max(mAudioSampleRate, 1));

    if (recording() || mEvaluationType != EvaluationType::Steady
        || !mVisualAudioSampleBaseInitialized) {
        mVisualAudioSampleBase = targetSampleBase;
        mVisualAudioSampleBaseInitialized = true;
        return mVisualAudioSampleBase;
    }

    const auto chunkFrameCount = static_cast<uint64_t>(mAudioChunkSize);
    const auto nextSampleBase = mVisualAudioSampleBase + chunkFrameCount;
    const auto bufferDuration =
        static_cast<uint64_t>(std::clamp(session().audioBufferSize, 50, 1000));
    const auto bufferFrameCount =
        (static_cast<uint64_t>(mAudioSampleRate) * bufferDuration + 999) / 1000;
    const auto correctionThreshold =
        std::max(chunkFrameCount * 2, bufferFrameCount / 2);

    // Prefer one texture frame per evaluation. Only correct persistent drift;
    // small playback-clock jitter must not change a feedback shader's input.
    if (targetSampleBase + correctionThreshold < nextSampleBase)
        return mVisualAudioSampleBase;

    if (targetSampleBase > nextSampleBase + correctionThreshold) {
        const auto distance = targetSampleBase - mVisualAudioSampleBase;
        auto chunkCount = distance / chunkFrameCount;
        if (distance % chunkFrameCount >= (chunkFrameCount + 1) / 2)
            ++chunkCount;
        mVisualAudioSampleBase += std::max<uint64_t>(chunkCount, 1)
            * chunkFrameCount;
    } else {
        mVisualAudioSampleBase = nextSampleBase;
    }
    return mVisualAudioSampleBase;
}

Bindings RenderSessionBase::mergeBindings(const BindingState &state)
{
    auto merged = Bindings{ };
    for (const Bindings &scope : state) {
        for (const auto &[name, binding] : scope.uniforms)
            merged.uniforms[name] = binding;
        for (const auto &[name, binding] : scope.samplers)
            if (binding.texture)
                merged.samplers[name] = binding;
        for (const auto &[name, binding] : scope.images)
            if (binding.texture)
                merged.images[name] = binding;
        for (const auto &[name, binding] : scope.buffers)
            if (binding.buffer)
                merged.buffers[name] = binding;
        for (const auto &[name, binding] : scope.subroutines)
            merged.subroutines[name] = binding;
    }
    return merged;
}

void RenderSessionBase::prepareSoundBuffer(Bindings &bindings,
    ItemId callItemId, int chunkIndex, BufferBase &buffer) const
{
    const auto sampleBase = mAudioSampleBase + chunkIndex * mAudioChunkSize;
    const auto header = SoundBufferHeader{
        .sampleBaseLow = static_cast<uint32_t>(sampleBase),
        .sampleBaseHigh = static_cast<uint32_t>(sampleBase >> 32),
        .sampleRate = static_cast<uint32_t>(mAudioSampleRate),
        .frameCount = static_cast<uint32_t>(mAudioChunkSize),
    };
    auto &data = buffer.writableData();
    std::memcpy(data.data(), &header, sizeof(header));

    const auto name = QString::fromLatin1(SoundBufferBindingName);
    bindings.buffers[name] =
        BufferBinding{ callItemId, name, &buffer, 0, "", "", 0 };
}

QByteArray RenderSessionBase::getSoundBufferData(const BufferBase &buffer) const
{
    return buffer.data().mid(sizeof(SoundBufferHeader));
}

void RenderSessionBase::writeAudioBuffers(
    std::vector<std::pair<SoundBufferKey, QByteArray>> soundBuffers)
{
    Q_ASSERT(onMainThread());
    if (mAudioChunkCount > 0) {
        auto batches = QMap<ItemId, QByteArray>{ };
        for (auto &[key, samples] : soundBuffers)
            batches[key.first].append(samples);
        const auto requestedByteCount = mMediaFrameWriter->audioFrameCount() * 2
            * sizeof(float);
        for (auto &samples : batches) {
            samples.truncate(requestedByteCount);
            mMediaFrameWriter->writeAudio(std::move(samples), 100);
        }
        mSessionModelCopy.forEachItem<Texture>([&](const Texture &texture) {
            if (!isAudioSource(texture.sourceType))
                return;

            auto width = 0, height = 0, depth = 0, layers = 0;
            evaluateTextureProperties(texture, &width, &height, &depth,
                &layers);
            const auto source = MediaSource{ texture.fileName,
                texture.sourceType, texture.target, QSize(width, 1) };
            auto samples = Singletons::mediaManager().audioSamples(source,
                mMediaFrameWriter->audioSampleBase(),
                mMediaFrameWriter->audioFrameCount());
            if (!samples.isEmpty()) {
                const auto volume = std::clamp(texture.audioVolume, 0, 100);
                mMediaFrameWriter->writeAudio(std::move(samples), volume);
            }
        });
    }
    mMediaFrameWriter->endFrame();
}

void RenderSessionBase::configure()
{
    Q_ASSERT(!onMainThread());

    if (mEvaluationType == EvaluationType::Reset) {
        mScriptSession.reset(new ScriptSession(this));
    } else {
        mScriptSession->resetMessages();
    }

    mScriptSession->beginSessionUpdate();
    mBindingValues.clear();

    // collect items to evaluate, since doing so can modify list
    auto itemsToEvaluate = QVector<const Item *>();
    mSessionModelCopy.forEachItem([&](const Item &item) {
        if (auto script = castItem<Script>(item)) {
            if (shouldExecute(script->executeOn, mEvaluationType))
                itemsToEvaluate.append(script);
        } else if (auto binding = castItem<Binding>(item)) {
            if (binding->bindingType == Binding::BindingType::Uniform)
                itemsToEvaluate.append(binding);
        }
    });

    auto &scriptEngine = mScriptSession->engine();
    for (const auto *item : std::as_const(itemsToEvaluate)) {
        if (auto script = castItem<Script>(item)) {
            auto source = QString();
            if (Singletons::fileCache().getSource(script->fileName, &source))
                scriptEngine.evaluateScript(source, script->fileName);
        } else if (auto binding = castItem<Binding>(item)) {
            // evaluate bindings with script objects, since they are setting globals
            evaluateBindingValues(*binding, scriptEngine);
        }
    }
}

void RenderSessionBase::evaluateBindingValues(const Binding &binding,
    ScriptEngine &scriptEngine)
{
    auto &values = mBindingValues[binding.id];
    if (values.isEmpty()) {
        if (auto it = mBindingValueOverrides.constFind(binding.id);
            it != mBindingValueOverrides.cend()) {
            values = scriptEngine.evaluateValues(*it, binding.id);
        } else {
            values = scriptEngine.evaluateValues(binding.values, binding.id);
        }
        // set global in script state
        scriptEngine.setGlobal(binding.name, values);
        mUsedItems += binding.id;
    }
}

void RenderSessionBase::configured()
{
    Q_ASSERT(onMainThread());
    if (mScriptSession)
        mScriptSession->endSessionUpdate();

    if (!recording() && mEvaluationType != EvaluationType::Steady
        && Singletons::synchronizeLogic().resetRenderSessionInvalidationState())
        mItemsChanged = true;
}

void RenderSessionBase::release()
{
    if (mScriptSession)
        mScriptSession->resetEngine();
}

const Session &RenderSessionBase::session() const
{
    return mSessionModelCopy.sessionItem();
}

QSet<ItemId> RenderSessionBase::usedItems() const
{
    QMutexLocker lock{ &mUsedItemsCopyMutex };
    return mUsedItemsCopy;
}

bool RenderSessionBase::usesMouseState() const
{
    return (mScriptSession && mScriptSession->usesMouseState());
}

bool RenderSessionBase::usesKeyboardState() const
{
    return (mScriptSession && mScriptSession->usesKeyboardState());
}

bool RenderSessionBase::usesViewportSize(const QString &fileName) const
{
    return (mScriptSession && mScriptSession->usesViewportSize(fileName));
}

bool RenderSessionBase::updatingTimerQueries() const
{
    return (mItemsChanged || mEvaluationType != EvaluationType::Steady);
}

void RenderSessionBase::setNextCommandQueueIndex(size_t index)
{
    mNextCommandQueueIndex = index;
}

int RenderSessionBase::getBufferSize(const Buffer &buffer)
{
    auto size = 1;
    for (const Item *item : buffer.items) {
        const auto &block = *static_cast<const Block *>(item);
        auto offset = 0, rowCount = 0;
        evaluateBlockProperties(block, &offset, &rowCount);
        size = std::max(size, offset + rowCount * getBlockStride(block));
    }
    return size;
}

void RenderSessionBase::invalidateCachedProperties()
{
    auto lock = QMutexLocker(&mPropertyCacheMutex);
    mPropertyCache.clear();
}

QList<int> RenderSessionBase::getCachedProperties(ItemId itemId)
{
    auto lock = QMutexLocker(&mPropertyCacheMutex);
    return mPropertyCache[itemId];
}

void RenderSessionBase::updateCachedProperties(ItemId itemId, QList<int> values)
{
    auto lock = QMutexLocker(&mPropertyCacheMutex);
    mPropertyCache[itemId] = values;
}

std::optional<size_t> RenderSessionBase::addTimeQuery(ItemId callId)
{
    if (mTimeQueryCallIds.size() >= maxTimeQueries)
        return std::nullopt;
    mTimeQueryCallIds.push_back(callId);
    return mTimeQueryCallIds.size() - 1;
}

void RenderSessionBase::evaluateBlockProperties(const Block &block, int *offset,
    int *rowCount, bool cached)
{
    if (auto values = getCachedProperties(block.id);
        cached && values.size() == 2) {
        *offset = values[0];
        *rowCount = values[1];
        return;
    }

    const auto evaluate = [&](ScriptEngine &engine) {
        Q_ASSERT(offset && rowCount);
        const auto guard = engine.beginSettingFirstError(block.id);
        *offset = engine.evaluateInt(block.offset, block.id);
        *rowCount = engine.evaluateInt(block.rowCount, block.id);
    };
    if (mScriptSession) {
        dispatchToRenderThread([&]() { evaluate(mScriptSession->engine()); });
        updateCachedProperties(block.id, { *offset, *rowCount });
    } else {
        evaluate(Singletons::defaultScriptEngine());
    }
}

void RenderSessionBase::evaluateTextureProperties(const Texture &texture,
    int *width, int *height, int *depth, int *layers, bool cached)
{
    if (auto values = getCachedProperties(texture.id);
        cached && values.size() == 4) {
        *width = values[0];
        *height = values[1];
        *depth = values[2];
        *layers = values[3];
        return;
    }

    const auto evaluate = [&](ScriptEngine &engine) {
        Q_ASSERT(width && height && depth && layers);
        const auto guard = engine.beginSettingFirstError(texture.id);
        *width = engine.evaluateInt(texture.width, texture.id);
        *height = engine.evaluateInt(texture.height, texture.id);
        *depth = engine.evaluateInt(texture.depth, texture.id);
        *layers = engine.evaluateInt(texture.layers, texture.id);
    };
    if (mScriptSession) {
        dispatchToRenderThread([&]() { evaluate(mScriptSession->engine()); });
        updateCachedProperties(texture.id,
            { *width, *height, *depth, *layers });
    } else {
        evaluate(Singletons::defaultScriptEngine());
    }
}

void RenderSessionBase::evaluateTargetProperties(const Target &target,
    int *width, int *height, int *layers, bool cached)
{
    if (auto values = getCachedProperties(target.id);
        cached && values.size() == 3) {
        *width = values[0];
        *height = values[1];
        *layers = values[2];
        return;
    }

    const auto evaluate = [&](ScriptEngine &engine) {
        Q_ASSERT(width && height && layers);
        const auto guard = engine.beginSettingFirstError(target.id);
        *width = engine.evaluateInt(target.defaultWidth, target.id);
        *height = engine.evaluateInt(target.defaultHeight, target.id);
        *layers = engine.evaluateInt(target.defaultLayers, target.id);
    };
    if (mScriptSession) {
        dispatchToRenderThread([&]() { evaluate(mScriptSession->engine()); });
        updateCachedProperties(target.id, { *width, *height, *layers });
    } else {
        evaluate(Singletons::defaultScriptEngine());
    }
}

void RenderSessionBase::obtainTimeQueryResults()
{
    // TODO: remove when message list performance issue is resolved
    if (!updatingTimerQueries()) {
        resetTimeQueries(0);
        mTimeQueryCallIds.clear();
        return;
    }

    mTimeQueryMessages.clear();
    auto total = std::chrono::duration<double>::zero();
    for (const auto &duration : resetTimeQueries(timeQueryCount())) {
        const auto itemId = mTimeQueryCallIds[mTimeQueryMessages.size()];
        mTimeQueryMessages.insert(itemId, MessageType::CallDuration,
            formatDuration(duration), false);
        total += duration;
    }
    mTimeQueryCallIds.clear();

    if (mTimeQueryMessages.size() > 1)
        mTimeQueryMessages.insert(MessageType::TotalDuration,
            formatDuration(total), false);
}
