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

void VKGeometrySet::prepare(VKContext &context, ScriptEngine &scriptEngine)
{
    if (mBottomLevelAs.isValid())
        return;

    auto buildOptions =
        KDGpu::BuildAccelerationStructureOptions::BuildOptions{ };
    auto geometryTypesAndCount = std::vector<
        KDGpu::AccelerationStructureOptions::GeometryTypeAndCount>();

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
    mBottomLevelAs = context.device.createAccelerationStructure(
        KDGpu::AccelerationStructureOptions{
            .type = KDGpu::AccelerationStructureType::BottomLevel,
            .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
            .geometryTypesAndCount = std::move(geometryTypesAndCount),
        });
    buildOptions.destinationStructure = mBottomLevelAs;
    context.commandRecorder->buildAccelerationStructures({
        .buildGeometryInfos = { std::move(buildOptions) },
    });
    memoryBarrier(*context.commandRecorder);
}
