#include "VKGeometrySet.h"

VKGeometrySet::VKGeometrySet(const Item &geometrySource)
    : mItemId(geometrySource.id)
{
    mUsedItems += geometrySource.id;
    for (auto item : geometrySource.items)
        if (auto geometry = castItem<Geometry>(item))
            mGeometries.push_back({
                .itemId = geometry->id,
                .type = geometry->geometryType,
                .opaque = geometry->opaque,
                .noDuplicateAnyHitInvocation =
                    geometry->noDuplicateAnyHitInvocation,
                .primitiveCount = geometry->count,
                .primitiveOffset = geometry->offset,
            });
}

bool VKGeometrySet::operator==(const VKGeometrySet &rhs) const
{
    return std::tie(mGeometries) == std::tie(rhs.mGeometries);
}

auto VKGeometrySet::getGeometry(int geometryIndex) -> VKGeometry &
{
    Q_ASSERT(geometryIndex >= 0
        && geometryIndex < static_cast<int>(mGeometries.size()));
    return mGeometries[geometryIndex];
}

void VKGeometrySet::setVertexBuffer(int geometryIndex, VKBuffer *buffer,
    const Block &block, VKRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();
    buffer->addUsage(
        KDGpu::BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyBit
        | KDGpu::BufferUsageFlagBits::ShaderDeviceAddressBit);

    auto &geometry = getGeometry(geometryIndex);
    geometry.vertexStride = getBlockStride(block);
    if (geometry.type == Geometry::GeometryType::AxisAlignedBoundingBoxes) {
        const auto expectedStride = sizeof(VkAabbPositionsKHR);
        if (geometry.vertexStride != expectedStride) {
            mMessages.insert(geometry.itemId,
                MessageType::InvalidGeometryStride,
                QStringLiteral("%1/%2 bytes")
                    .arg(geometry.vertexStride)
                    .arg(expectedStride));
            return;
        }
    }

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);
    geometry.vertexBuffer = buffer;
    geometry.vertexBufferOffset = static_cast<size_t>(offset);
    geometry.vertexCount = static_cast<uint32_t>(rowCount);
}

void VKGeometrySet::setIndexBuffer(int geometryIndex, VKBuffer *buffer,
    const Block &block, VKRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();
    buffer->addUsage(
        KDGpu::BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyBit
        | KDGpu::BufferUsageFlagBits::ShaderDeviceAddressBit);

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);
    auto indexSize = 0;
    for (auto item : block.items)
        if (auto field = castItem<Field>(item)) {
            indexSize += getFieldSize(*field);
            mUsedItems += field->id;
        }
    const auto indexType = getKDIndexType(indexSize);
    if (!indexType || !indexSize) {
        mMessages.insert(block.id, MessageType::InvalidIndexType,
            QStringLiteral("%1 bytes").arg(indexSize));
        return;
    }

    auto &geometry = getGeometry(geometryIndex);
    geometry.indexBuffer = buffer;
    geometry.indexBufferOffset = static_cast<size_t>(offset);
    geometry.indexType = *indexType;
    const auto indicesPerRow = getBlockStride(block) / indexSize;
    geometry.indexCount = static_cast<uint32_t>(rowCount) * indicesPerRow;
}

void VKGeometrySet::setTransformBuffer(int geometryIndex, VKBuffer *buffer,
    const Block &block, VKRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();
    buffer->addUsage(
        KDGpu::BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyBit
        | KDGpu::BufferUsageFlagBits::ShaderDeviceAddressBit);

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);
    auto &geometry = getGeometry(geometryIndex);
    geometry.transformBuffer = buffer;
    geometry.transformBufferOffset = static_cast<size_t>(offset);
}

void VKGeometrySet::memoryBarrier(KDGpu::CommandRecorder &commandRecorder)
{
    commandRecorder.memoryBarrier(KDGpu::MemoryBarrierOptions{
        .srcStages = KDGpu::PipelineStageFlagBit::AccelerationStructureBuildBit,
        .dstStages = KDGpu::PipelineStageFlagBit::AccelerationStructureBuildBit,
        .memoryBarriers = {
            {
                .srcMask = KDGpu::AccessFlagBit::AccelerationStructureWriteBit,
                .dstMask = KDGpu::AccessFlagBit::AccelerationStructureReadBit,
            },
        },
    });
}

void VKGeometrySet::memoryBarrierBeforeUpdate(
    KDGpu::CommandRecorder &commandRecorder)
{
    commandRecorder.memoryBarrier(KDGpu::MemoryBarrierOptions{
        .srcStages = KDGpu::PipelineStageFlagBit::AllCommandsBit,
        .dstStages =
            KDGpu::PipelineStageFlagBit::AccelerationStructureBuildBit,
        .memoryBarriers = {
            {
                .srcMask = KDGpu::AccessFlagBit::AccelerationStructureReadBit,
                .dstMask =
                    KDGpu::AccessFlagBit::AccelerationStructureReadBit
                    | KDGpu::AccessFlagBit::AccelerationStructureWriteBit,
            },
        },
    });
}

void VKGeometrySet::prepare(VKContext &context, ScriptEngine &scriptEngine)
{
    auto buildOptions =
        KDGpu::BuildAccelerationStructureOptions::BuildOptions{ };
    auto geometryTypesAndCount = std::vector<
        KDGpu::AccelerationStructureOptions::GeometryTypeAndCount>();
    auto inputKey = std::vector<uint64_t>{
        static_cast<uint64_t>(mGeometries.size())
    };
    auto updateKey = inputKey;
    const auto append = [](auto &key,
                            std::initializer_list<uint64_t> values) {
        key.insert(key.end(), values.begin(), values.end());
    };

    for (const auto &geometry : mGeometries) {
        mUsedItems += geometry.itemId;
        if (!geometry.vertexBuffer) {
            mMessages.insert(geometry.itemId, MessageType::BufferNotSet,
                "Vertices");
            return;
        }
        auto &vertexBuffer = *geometry.vertexBuffer;
        vertexBuffer.prepareAccelerationStructureGeometry(context);
        Q_ASSERT(vertexBuffer.buffer().bufferDeviceAddress());

        auto indexBufferHandle = KDGpu::Handle<KDGpu::Buffer_t>();
        auto transformBufferHandle = KDGpu::Handle<KDGpu::Buffer_t>();
        auto maxPrimitiveCount = 0u;
        using GeometryType = Geometry::GeometryType;
        if (geometry.type == GeometryType::AxisAlignedBoundingBoxes) {
            maxPrimitiveCount = geometry.vertexCount;
        } else if (geometry.type == GeometryType::Triangles) {
            maxPrimitiveCount = geometry.vertexCount / 3;
            if (auto indexBuffer = geometry.indexBuffer) {
                indexBuffer->prepareAccelerationStructureGeometry(context);
                Q_ASSERT(indexBuffer->buffer().bufferDeviceAddress());
                indexBufferHandle = indexBuffer->buffer();
                maxPrimitiveCount = geometry.indexCount / 3;
            }
            if (auto transformBuffer = geometry.transformBuffer) {
                transformBuffer->prepareAccelerationStructureGeometry(context);
                Q_ASSERT(transformBuffer->buffer().bufferDeviceAddress());
                transformBufferHandle = transformBuffer->buffer();
            }
        } else {
            Q_UNREACHABLE();
        }

        const auto primitiveCount = (geometry.primitiveCount.isEmpty()
                ? maxPrimitiveCount
                : scriptEngine.evaluateUInt(
                      geometry.primitiveCount, geometry.itemId));
        const auto primitiveOffset = scriptEngine.evaluateUInt(
            geometry.primitiveOffset, geometry.itemId);
        if (primitiveCount > maxPrimitiveCount) {
            mMessages.insert(geometry.itemId, MessageType::CountExceeded,
                QStringLiteral("%1 > %2")
                    .arg(primitiveCount)
                    .arg(maxPrimitiveCount));
            return;
        }

        auto geometryFlags = KDGpu::AccelerationStructureGeometryFlags{ };
        if (geometry.opaque)
            geometryFlags |=
                KDGpu::AccelerationStructureGeometryFlagBits::Opaque;
        if (geometry.noDuplicateAnyHitInvocation)
            geometryFlags |= KDGpu::AccelerationStructureGeometryFlagBits::
                NoDuplicateAnyHitInvocation;

        if (geometry.type == GeometryType::AxisAlignedBoundingBoxes) {
            const auto aabbsGeometry =
                KDGpu::AccelerationStructureGeometryAabbsData{
                    .flags = geometryFlags,
                    .data = vertexBuffer.buffer(),
                    .stride = geometry.vertexStride,
                    .dataOffset = geometry.vertexBufferOffset,
                };
            geometryTypesAndCount.push_back({
                .geometry = aabbsGeometry,
                .maxPrimitiveCount = primitiveCount,
            });
            buildOptions.geometries.push_back(aabbsGeometry);

            const auto values = std::initializer_list<uint64_t>{
                static_cast<uint64_t>(geometry.type),
                static_cast<uint64_t>(geometryFlags.toInt()), primitiveCount,
                primitiveOffset,
                vertexBuffer.buffer().bufferDeviceAddress()
                    + geometry.vertexBufferOffset,
                geometry.vertexStride
            };
            append(inputKey, values);
            append(updateKey, values);
            inputKey.push_back(vertexBuffer.contentRevision());
        } else {
            const auto triangleGeometry =
                KDGpu::AccelerationStructureGeometryTrianglesData{
                    .flags = geometryFlags,
                    .vertexFormat = KDGpu::Format::R32G32B32_SFLOAT,
                    .vertexData = vertexBuffer.buffer(),
                    .vertexStride = geometry.vertexStride,
                    .vertexDataOffset = geometry.vertexBufferOffset,
                    .maxVertex = std::max(geometry.vertexCount, 1u) - 1u,
                    .indexType = geometry.indexType,
                    .indexData = indexBufferHandle,
                    .indexDataOffset = geometry.indexBufferOffset,
                    .transformData = transformBufferHandle,
                    .transformDataOffset = geometry.transformBufferOffset,
                };
            geometryTypesAndCount.push_back({
                .geometry = triangleGeometry,
                .maxPrimitiveCount = primitiveCount,
            });
            buildOptions.geometries.push_back(triangleGeometry);

            const auto indexAddress = geometry.indexBuffer
                ? geometry.indexBuffer->buffer().bufferDeviceAddress()
                    + geometry.indexBufferOffset
                : 0;
            const auto transformAddress = geometry.transformBuffer
                ? geometry.transformBuffer->buffer().bufferDeviceAddress()
                    + geometry.transformBufferOffset
                : 0;
            const auto values = std::initializer_list<uint64_t>{
                static_cast<uint64_t>(geometry.type),
                static_cast<uint64_t>(geometryFlags.toInt()), primitiveCount,
                primitiveOffset,
                static_cast<uint64_t>(triangleGeometry.vertexFormat),
                triangleGeometry.vertexDataOffset,
                triangleGeometry.vertexStride, triangleGeometry.maxVertex,
                static_cast<uint64_t>(triangleGeometry.indexType),
                triangleGeometry.indexDataOffset,
                triangleGeometry.transformDataOffset,
                vertexBuffer.buffer().bufferDeviceAddress()
                    + geometry.vertexBufferOffset,
                indexAddress, transformAddress
            };
            append(inputKey, values);
            append(updateKey, values);
            inputKey.push_back(vertexBuffer.contentRevision());
            const auto indexRevision = geometry.indexBuffer
                ? geometry.indexBuffer->contentRevision()
                : 0;
            inputKey.push_back(indexRevision);
            updateKey.push_back(indexRevision);
            inputKey.push_back(geometry.transformBuffer
                    ? geometry.transformBuffer->contentRevision()
                    : 0);
        }
        buildOptions.buildRangeInfos.push_back({
            .primitiveCount = primitiveCount,
            .primitiveOffset = primitiveOffset,
            .firstVertex = 0,
            .transformOffset = 0,
        });
    }

    if (buildOptions.geometries.empty())
        return;

    if (mBottomLevelAs.isValid() && mInputKey == inputKey)
        return;

    const auto compatible =
        mBottomLevelAs.isValid() && mUpdateKey == updateKey;
    const auto update = compatible && mUpdateCapable;
    const auto updateCapable = mUpdateCapable || compatible;

    if (update) {
        memoryBarrierBeforeUpdate(*context.commandRecorder);
        buildOptions.mode = KDGpu::BuildAccelerationStructureMode::Update;
        buildOptions.sourceStructure = mBottomLevelAs;
        buildOptions.destinationStructure = mBottomLevelAs;
    } else {
        auto flags = KDGpu::AccelerationStructureFlags{
            KDGpu::AccelerationStructureFlagBits::PreferFastTrace
        };
        if (updateCapable)
            flags |= KDGpu::AccelerationStructureFlagBits::AllowUpdate;
        auto bottomLevelAs = context.device.createAccelerationStructure(
            KDGpu::AccelerationStructureOptions{
            .type = KDGpu::AccelerationStructureType::BottomLevel,
            .flags = flags,
            .geometryTypesAndCount = std::move(geometryTypesAndCount),
        });
        buildOptions.destinationStructure = bottomLevelAs;
        if (mBottomLevelAs.isValid())
            context.stagingAccelerationStructures.push_back(
                std::move(mBottomLevelAs));
        mBottomLevelAs = std::move(bottomLevelAs);
        mUpdateCapable = updateCapable;
    }

    context.commandRecorder->buildAccelerationStructures({
        .buildGeometryInfos = { std::move(buildOptions) },
    });
    memoryBarrier(*context.commandRecorder);
    mInputKey = std::move(inputKey);
    mUpdateKey = std::move(updateKey);
    ++mRevision;
}
