
#include "VKAccelerationStructure.h"

VKAccelerationStructure::VKAccelerationStructure(
    const AccelerationStructure &accelStruct)
{
    mUsedItems += accelStruct.id;

    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item)) {
            auto geometries = std::vector<VKGeometry>();
            for (auto item : instance->items)
                if (auto geometry = castItem<Geometry>(item)) {
                    geometries.push_back({
                        .itemId = geometry->id,
                        .type = geometry->geometryType,
                        .primitiveCount = geometry->count,
                        .primitiveOffset = geometry->offset,
                    });
                }
            mInstances.push_back({
                .itemId = item->id,
                .geometries = std::move(geometries),
            });
        }
}

bool VKAccelerationStructure::operator==(
    const VKAccelerationStructure &rhs) const
{
    return std::tie(mInstances) == std::tie(rhs.mInstances);
}

auto VKAccelerationStructure::getGeometry(int instanceIndex, int geometryIndex)
    -> VKGeometry &
{
    Q_ASSERT(instanceIndex >= 0
        && instanceIndex < static_cast<int>(mInstances.size()));
    auto &instance = mInstances[instanceIndex];
    Q_ASSERT(geometryIndex >= 0
        && geometryIndex < static_cast<int>(instance.geometries.size()));
    return instance.geometries[geometryIndex];
}

void VKAccelerationStructure::setVertexBuffer(int instanceIndex,
    int geometryIndex, VKBuffer *buffer, const Block &block,
    VKRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();

    buffer->addUsage(
        KDGpu::BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyBit
        | KDGpu::BufferUsageFlagBits::ShaderDeviceAddressBit);

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
    geometry.vertexStride = getBlockStride(block);

    if (geometry.type == Geometry::GeometryType::AxisAlignedBoundingBoxes) {
        const auto expectedStride = sizeof(VkAabbPositionsKHR);
        if (geometry.vertexStride != expectedStride) {
            mMessages += MessageList::insert(geometry.itemId,
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

void VKAccelerationStructure::setIndexBuffer(int instanceIndex,
    int geometryIndex, VKBuffer *buffer, const Block &block,
    VKRenderSession &renderSession)
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
        mMessages += MessageList::insert(block.id,
            MessageType::InvalidIndexType,
            QStringLiteral("%1 bytes").arg(indexSize));
        return;
    }

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
    geometry.indexBuffer = buffer;
    geometry.indexBufferOffset = static_cast<size_t>(offset);
    geometry.indexType = *indexType;
    const auto indicesPerRow = getBlockStride(block) / indexSize;
    geometry.indexCount = static_cast<uint32_t>(rowCount) * indicesPerRow;
}

void VKAccelerationStructure::memoryBarrier(
    KDGpu::CommandRecorder &commandRecorder)
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

void VKAccelerationStructure::prepare(VKContext &context,
    ScriptEngine &scriptEngine)
{
    if (mTopLevelAs.isValid())
        return;

    const auto getTransformValues = [&](ItemId itemId,
                                        const QString &transform) {
        if (transform.isEmpty())
            return ScriptValueList{};
        auto values = scriptEngine.evaluateValues(transform, itemId, mMessages);
        if (values.size() != 12 && values.size() != 16)
            mMessages += MessageList::insert(itemId,
                MessageType::UniformComponentMismatch,
                QString("(%1/12 or 16)").arg(values.count()));
        values.resize(12);
        return values;
    };

    mBottomLevelAs.clear();

    auto blasBuildGeometryInfos =
        std::vector<KDGpu::BuildAccelerationStructureOptions::BuildOptions>();
    auto geometryInstances =
        KDGpu::AccelerationStructureGeometryInstancesData{};

    // create one BLAS per instance
    auto instanceIndex = 0u;
    for (const auto &instance : mInstances) {
        mUsedItems += instance.itemId;

        auto &blasBuildOptions = blasBuildGeometryInfos.emplace_back();
        auto geometryTypesAndCount = std::vector<
            KDGpu::AccelerationStructureOptions::GeometryTypeAndCount>();

        for (const auto &geometry : instance.geometries) {
            mUsedItems += geometry.itemId;

            if (!geometry.vertexBuffer) {
                mMessages += MessageList::insert(geometry.itemId,
                    MessageType::BufferNotSet, "Vertices");
                return;
            }
            auto &vertexBuffer = *geometry.vertexBuffer;
            vertexBuffer.prepareAccelerationStructureGeometry(context);
            Q_ASSERT(vertexBuffer.buffer().bufferDeviceAddress());

            auto indexBufferHandle = KDGpu::Handle<KDGpu::Buffer_t>();
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
            } else {
                Q_UNREACHABLE();
            }

            const auto primitiveCount = (geometry.primitiveCount.isEmpty()
                    ? maxPrimitiveCount
                    : scriptEngine.evaluateUInt(geometry.primitiveCount,
                          geometry.itemId, mMessages));
            const auto primitiveOffset = scriptEngine.evaluateUInt(
                geometry.primitiveOffset, geometry.itemId, mMessages);

            if (primitiveCount > maxPrimitiveCount) {
                mMessages += MessageList::insert(geometry.itemId,
                    MessageType::CountExceeded,
                    QStringLiteral("%1 > %2")
                        .arg(primitiveCount)
                        .arg(maxPrimitiveCount));
                return;
            }

            if (geometry.type == GeometryType::AxisAlignedBoundingBoxes) {
                const auto aabbsGeometry =
                    KDGpu::AccelerationStructureGeometryAabbsData{
                        .data = vertexBuffer.buffer(),
                        .stride = geometry.vertexStride,
                        .dataOffset = geometry.vertexBufferOffset,
                    };
                geometryTypesAndCount.push_back({
                    .geometry = aabbsGeometry,
                    .maxPrimitiveCount = primitiveCount,
                });
                blasBuildOptions.geometries.push_back(aabbsGeometry);

            } else if (geometry.type == GeometryType::Triangles) {
                const auto triangleGeometry =
                    KDGpu::AccelerationStructureGeometryTrianglesData{
                        .vertexFormat = KDGpu::Format::R32G32B32_SFLOAT,
                        .vertexData = vertexBuffer.buffer(),
                        .vertexStride = geometry.vertexStride,
                        .vertexDataOffset = geometry.vertexBufferOffset,
                        .maxVertex = std::max(geometry.vertexCount, 1u) - 1u,
                        .indexType = geometry.indexType,
                        .indexData = indexBufferHandle,
                        .indexDataOffset = geometry.indexBufferOffset,
                        .transformData = {},
                        .transformDataOffset = 0,
                    };
                geometryTypesAndCount.push_back({
                    .geometry = triangleGeometry,
                    .maxPrimitiveCount = primitiveCount,
                });
                blasBuildOptions.geometries.push_back(triangleGeometry);
            }
            blasBuildOptions.buildRangeInfos.push_back({
                .primitiveCount = primitiveCount,
                .primitiveOffset = primitiveOffset,
                .firstVertex = 0,
                .transformOffset = 0,
            });
        }

        auto &bottomLevelAs = mBottomLevelAs.emplace_back();
        bottomLevelAs = context.device.createAccelerationStructure(
            KDGpu::AccelerationStructureOptions{
                .type = KDGpu::AccelerationStructureType::BottomLevel,
                .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
                .geometryTypesAndCount = std::move(geometryTypesAndCount),
            });

        blasBuildOptions.destinationStructure = bottomLevelAs;

        auto &geometryInstance = geometryInstances.data.emplace_back(
            KDGpu::AccelerationStructureGeometryInstance{
                .transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
                .instanceCustomIndex = instanceIndex++,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags =
                    KDGpu::GeometryInstanceFlagBits::TriangleFacingCullDisable,
                .accelerationStructure = bottomLevelAs,
            });

        const auto transform =
            getTransformValues(instance.itemId, instance.transform);
        for (auto i = 0; i < transform.size(); ++i)
            geometryInstance.transform[i / 4][i % 4] = transform[i];
    }

    // build all BLASs
    context.commandRecorder->buildAccelerationStructures({
        .buildGeometryInfos = std::move(blasBuildGeometryInfos),
    });

    memoryBarrier(*context.commandRecorder);

    // one TLAS with an instances list
    mTopLevelAs = context.device.createAccelerationStructure({
        .type = KDGpu::AccelerationStructureType::TopLevel,
        .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
        .geometryTypesAndCount = {
            {
                .geometry = geometryInstances,
                .maxPrimitiveCount = static_cast<uint32_t>(geometryInstances.data.size()),
            },
        },
    });

    context.commandRecorder->buildAccelerationStructures({
        .buildGeometryInfos = {
            {
                .geometries = { geometryInstances },
                .destinationStructure = mTopLevelAs,
                .buildRangeInfos = {
                    {
                        .primitiveCount = static_cast<uint32_t>(geometryInstances.data.size()),
                        .primitiveOffset = 0,
                        .firstVertex = 0,
                        .transformOffset = 0,
                    },
                },
            },
        },
    });
}
