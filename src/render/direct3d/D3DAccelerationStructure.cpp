
#include "D3DAccelerationStructure.h"

D3DAccelerationStructure::D3DAccelerationStructure(
    const AccelerationStructure &accelStruct)
    : mItemId(accelStruct.id)
{
    mUsedItems += accelStruct.id;

    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item)) {
            auto geometries = std::vector<D3DGeometry>();
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
                .transform = instance->transform,
                .geometries = std::move(geometries),
            });
        }
}

bool D3DAccelerationStructure::operator==(
    const D3DAccelerationStructure &rhs) const
{
    return std::tie(mInstances) == std::tie(rhs.mInstances);
}

auto D3DAccelerationStructure::getGeometry(int instanceIndex, int geometryIndex)
    -> D3DGeometry &
{
    Q_ASSERT(instanceIndex >= 0
        && instanceIndex < static_cast<int>(mInstances.size()));
    auto &instance = mInstances[instanceIndex];
    Q_ASSERT(geometryIndex >= 0
        && geometryIndex < static_cast<int>(instance.geometries.size()));
    return instance.geometries[geometryIndex];
}

void D3DAccelerationStructure::setVertexBuffer(int instanceIndex,
    int geometryIndex, D3DBuffer *buffer, const Block &block,
    D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();

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

void D3DAccelerationStructure::setIndexBuffer(int instanceIndex,
    int geometryIndex, D3DBuffer *buffer, const Block &block,
    D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);

    auto indexSize = 0;
    for (auto item : block.items)
        if (auto field = castItem<Field>(item)) {
            indexSize += getFieldSize(*field);
            mUsedItems += field->id;
        }

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
    geometry.indexBuffer = buffer;
    geometry.indexBufferOffset = static_cast<size_t>(offset);
    geometry.indexSize = indexSize;
    const auto indicesPerRow = getBlockStride(block) / indexSize;
    geometry.indexCount = static_cast<uint32_t>(rowCount) * indicesPerRow;
}

void D3DAccelerationStructure::setTransformBuffer(int instanceIndex,
    int geometryIndex, D3DBuffer *buffer, const Block &block,
    D3DRenderSession &renderSession)
{
    if (!buffer)
        return;

    mUsedItems += buffer->usedItems();

    auto offset = 0;
    auto rowCount = 0;
    renderSession.evaluateBlockProperties(block, &offset, &rowCount);

    auto &geometry = getGeometry(instanceIndex, geometryIndex);
    geometry.transformBuffer = buffer;
    geometry.transformBufferOffset = static_cast<size_t>(offset);
}
