
#include "VKAccelerationStructure.h"

VKAccelerationStructure::VKAccelerationStructure(
    const AccelerationStructure &accelStruct, ScriptEngine &scriptEngine)
{
    mUsedItems += accelStruct.id;

    const auto getTransform = [&](const Instance &instance) {
        if (instance.transform.isEmpty())
            return ScriptValueList{};
        auto values = scriptEngine.evaluateValues(instance.transform,
            instance.id, mMessages);
        if (values.size() != 12 && values.size() != 16)
            mMessages += MessageList::insert(instance.id,
                MessageType::UniformComponentMismatch,
                QString("(%1/12 or 16)").arg(values.count()));
        values.resize(12);
        return values;
    };

    const auto getGeometries = [&](const Instance &instance) {
        auto geometries = std::vector<VKGeometry>();
        for (auto item : instance.items)
            if (auto geometry = castItem<Geometry>(item))
                geometries.push_back({
                    .itemId = geometry->id,
                    .type = geometry->geometryType,
                    .primitiveCount = scriptEngine.evaluateUInt(geometry->count,
                        geometry->id, mMessages),
                    .primitiveOffset = scriptEngine.evaluateUInt(
                        geometry->offset, geometry->id, mMessages),
                });
        return geometries;
    };

    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item))
            mInstances.push_back({
                .itemId = item->id,
                .transform = getTransform(*instance),
                .geometries = getGeometries(*instance),
            });
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
    geometry.maxVertexIndex = static_cast<size_t>(std::max(rowCount, 1) - 1);
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
    if (!indexType) {
        mMessages += MessageList::insert(block.id,
            MessageType::InvalidIndexType,
            QStringLiteral("%1 bytes").arg(indexSize));
        return;
    }

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
    geometry.indexBuffer = buffer;
    geometry.indexBufferOffset = static_cast<size_t>(offset);
    geometry.indexType = *indexType;
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

void VKAccelerationStructure::prepare(VKContext &context)
{
    if (mTopLevelAs.isValid())
        return;

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

            using GeometryType = Geometry::GeometryType;
            if (geometry.type == GeometryType::AxisAlignedBoundingBoxes) {
                const auto aabbsGeometry =
                    KDGpu::AccelerationStructureGeometryAabbsData{
                        .data = vertexBuffer.buffer(),
                        .stride = geometry.vertexStride,
                        .dataOffset = geometry.vertexBufferOffset,
                    };
                geometryTypesAndCount.push_back({
                    .geometry = aabbsGeometry,
                    .maxPrimitiveCount = geometry.primitiveCount,
                });

                blasBuildOptions.geometries.push_back(aabbsGeometry);

            } else if (geometry.type == GeometryType::Triangles) {
                auto indexBufferHandle = KDGpu::Handle<KDGpu::Buffer_t>();
                if (auto indexBuffer = geometry.indexBuffer) {
                    indexBuffer->prepareAccelerationStructureGeometry(context);
                    indexBufferHandle = indexBuffer->buffer();
                }
                const auto triangleGeometry =
                    KDGpu::AccelerationStructureGeometryTrianglesData{
                        .vertexFormat = KDGpu::Format::R32G32B32_SFLOAT,
                        .vertexData = vertexBuffer.buffer(),
                        .vertexStride = geometry.vertexStride,
                        .vertexDataOffset = geometry.vertexBufferOffset,
                        .maxVertex = geometry.maxVertexIndex,
                        .indexType = geometry.indexType,
                        .indexData = indexBufferHandle,
                        .indexDataOffset = geometry.indexBufferOffset,
                        .transformData = {},
                        .transformDataOffset = 0,
                    };
                geometryTypesAndCount.push_back({
                    .geometry = triangleGeometry,
                    .maxPrimitiveCount = geometry.primitiveCount,
                });

                blasBuildOptions.geometries.push_back(triangleGeometry);
            }
            blasBuildOptions.buildRangeInfos.push_back({
                .primitiveCount = geometry.primitiveCount,
                .primitiveOffset = geometry.primitiveOffset,
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
        for (auto i = 0; i < instance.transform.size(); ++i)
            geometryInstance.transform[i / 4][i % 4] = instance.transform[i];
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
