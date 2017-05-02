#include "RenderCall.h"
#include "Singletons.h"
#include "MessageList.h"
#include "GLProgram.h"
#include "GLPrimitives.h"
#include "GLFramebuffer.h"

struct GLCall
{
    Call::Type type;
    GLuint numGroupsX;
    GLuint numGroupsY;
    GLuint numGroupsZ;
};

struct RenderCall::Input
{
    GLCall call;
    GLProgram program;
    GLPrimitives primitives;
    GLFramebuffer framebuffer;
};

struct RenderCall::Cache
{
    GLCall call;
    GLProgram program;
    GLPrimitives primitives;
    GLFramebuffer framebuffer;
};

struct RenderCall::Output
{
    QSet<ItemId> usedItems;
    MessageList messages;
    QList<std::pair<QString, QImage>> images;
    QList<std::pair<QString, QByteArray>> buffers;
};

static void createInput(PrepareContext &context, ItemId callId,
    RenderCall::Input &input)
{
    auto call = context.session.findItem<Call>(callId);
    if (!call)
        return;

    input.call = {
        call->type,
        GLuint(call->numGroupsX),
        GLuint(call->numGroupsY),
        GLuint(call->numGroupsZ)
    };
    context.usedItems += call->id;

    if (auto program = context.session.findItem<Program>(call->programId))
        input.program.initialize(context, *program);

    if (auto primitives = context.session.findItem<Primitives>(call->primitivesId))
        input.primitives.initialize(context, *primitives);

    if (auto framebuffer = context.session.findItem<Framebuffer>(call->framebufferId))
        input.framebuffer.initialize(context, *framebuffer);

    context.session.forEachItemScoped(context.session.index(call),
        [&](const Item& item) {
            switch (item.itemType) {
                case ItemType::Texture:
                    input.program.addTextureBinding(context,
                        *castItem<Texture>(&item));
                    break;

                case ItemType::Buffer:
                    input.program.addBufferBinding(context,
                        *castItem<Buffer>(&item));
                    break;

                case ItemType::Sampler:
                    input.program.addSamplerBinding(context,
                        *castItem<Sampler>(&item));
                    break;

                case ItemType::Binding:
                    input.program.addBinding(context,
                        *castItem<Binding>(&item));
                    break;

                default:
                    break;
            }
        });
}

static void renderCached(RenderContext &context,
    RenderCall::Input &input,
    RenderCall::Cache &cache,
    RenderCall::Output &output)
{
    cache.call = std::move(input.call);
    cache.program.cache(context, std::move(input.program));
    cache.primitives.cache(context, std::move(input.primitives));
    cache.framebuffer.cache(context, std::move(input.framebuffer));

    if (cache.call.type == Call::Draw) {
        if (cache.framebuffer.bind(context)) {
            if (cache.program.bind(context)) {
                cache.primitives.draw(context, cache.program);
                cache.program.unbind(context);
            }
            cache.framebuffer.unbind(context);
        }
    }
    else if (cache.call.type == Call::Compute) {
        if (context.gl43)
            if (cache.program.bind(context)) {
                context.gl43->glDispatchCompute(
                    cache.call.numGroupsX,
                    cache.call.numGroupsY,
                    cache.call.numGroupsZ);
                cache.program.unbind(context);
            }
    }

    output.images += cache.framebuffer.getModifiedImages(context);
    output.images += cache.program.getModifiedImages(context);
    output.buffers += cache.program.getModifiedBuffers(context);
}

static void applyOutput(RenderCall::Output &output)
{
    for (const auto& image : output.images) {
        const auto& fileName = image.first;
        Singletons::fileManager().openImageEditor(fileName, false);
        if (auto file = Singletons::fileManager().findImageFile(fileName))
            file->replace(image.second);
    }

    for (const auto& buffer : output.buffers) {
        const auto& fileName = buffer.first;
        Singletons::fileManager().openBinaryEditor(fileName, false);
        if (auto file = Singletons::fileManager().findBinaryFile(fileName))
            file->replace(buffer.second);
    }
}

RenderCall::RenderCall(ItemId callId, QObject *parent)
    : RenderTask(parent)
    , mCallId(callId)
    , mCache(new Cache())
{
}

RenderCall::~RenderCall()
{
    releaseResources();
}

void RenderCall::prepare()
{
    mInput.reset(new Input());
    mOutput.reset(new Output());
    auto context = PrepareContext(
        &mOutput->usedItems, &mOutput->messages);
    createInput(context, mCallId, *mInput);
}

void RenderCall::render(QOpenGLContext &glContext)
{
    auto context = RenderContext(glContext,
        &mOutput->usedItems, &mOutput->messages);
    if (context)
        renderCached(context, *mInput, *mCache, *mOutput);
}

void RenderCall::finish()
{
    applyOutput(*mOutput);
    mUsedItems = std::move(mOutput->usedItems);
    mMessages = std::move(mOutput->messages);
}

void RenderCall::release(QOpenGLContext &glContext)
{
    Q_UNUSED(glContext);
    mCache.reset();
}
