
#include "VKAccelerationStructure.h"

bool operator==(const VKAccelerationStructure::VKInstance &a,
    const VKAccelerationStructure::VKInstance &b)
{
    const auto tie = [](const VKAccelerationStructure::VKInstance &a) {
        return std::tie(a.geometryType, a.vertexBuffer, a.indexBuffer);
    };
    return tie(a) == tie(b);
}

VKAccelerationStructure::VKAccelerationStructure(
    const AccelerationStructure &accelStruct, ScriptEngine &scriptEngine)
    : mItemId(accelStruct.id)
{
    const auto getTransform = [&](const Instance &instance) {
        if (instance.transform.isEmpty())
            return ScriptValueList{};
        auto values = scriptEngine.evaluateValues(instance.transform,
            instance.id, mMessages);
        if (values.size() != 16)
            mMessages += MessageList::insert(instance.id,
                MessageType::UniformComponentMismatch,
                QString("(%1/16)").arg(values.count()));
        values.resize(16);
        return values;
    };

    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item))
            mInstances.push_back({
                .transform = getTransform(*instance),
                .geometryType = instance->geometryType,
            });
}

bool VKAccelerationStructure::operator==(
    const VKAccelerationStructure &rhs) const
{
    return std::tie(mInstances) == std::tie(rhs.mInstances);
}

void VKAccelerationStructure::setBuffers(int index, VKBuffer *vertexBuffer,
    VKBuffer *indexBuffer)
{
    Q_ASSERT(index >= 0 && index < static_cast<int>(mInstances.size()));
    mInstances[index].vertexBuffer = vertexBuffer;
    mInstances[index].indexBuffer = indexBuffer;
}

void VKAccelerationStructure::createBottomLevelAsAabbs(VKContext &context,
    const VKInstance &instance)
{
    auto &aabbBuffer = *instance.vertexBuffer;
    aabbBuffer.prepareAccelerationStructureGeometry(context);

    const auto aabbGeometry = KDGpu::AccelerationStructureGeometryAabbsData{
        .data = aabbBuffer.buffer(),
        .stride = sizeof(VkAabbPositionsKHR),
    };
    const auto aabbCount =
        static_cast<uint32_t>(aabbBuffer.size() / sizeof(VkAabbPositionsKHR));

    auto &bottomLevelAs = mBottomLevelAs.emplace_back();
    bottomLevelAs = context.device.createAccelerationStructure(
          KDGpu::AccelerationStructureOptions{
              .type = KDGpu::AccelerationStructureType::BottomLevel,
              .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
              .geometryTypesAndCount = {
                  {
                      .geometry = aabbGeometry,
                      .maxPrimitiveCount = aabbCount,
                  },
              },
      });

    context.commandRecorder->buildAccelerationStructures({
          .buildGeometryInfos = {
              {
                  .geometries = { aabbGeometry },
                  .destinationStructure = bottomLevelAs,
                  .buildRangeInfos = {
                      {
                          .primitiveCount = aabbCount,
                          .primitiveOffset = 0,
                          .firstVertex = 0,
                          .transformOffset = 0,
                      },
                  },
              },
          },
      });
}

void VKAccelerationStructure::createBottomLevelAsTriangles(VKContext &context,
    const VKInstance &instance)
{
    auto &vertices = *instance.vertexBuffer;
    vertices.prepareAccelerationStructureGeometry(context);

    // TODO:
    const auto stride = sizeof(float) * 9;
    const auto vertexCount = static_cast<uint32_t>(vertices.size() / stride);
    const auto triangleCount = vertexCount / 3;

    const auto maxVertexIndex = std::max(vertexCount, 1u) - 1u;
    const auto triangleGeometry =
        KDGpu::AccelerationStructureGeometryTrianglesData{
            .vertexFormat = KDGpu::Format::R32G32B32_SFLOAT,
            .vertexData = vertices.buffer(),
            .vertexStride = stride,
            .vertexDataOffset = 0,
            .maxVertex = maxVertexIndex,
            .indexType = KDGpu::IndexType::None,
            .indexData = {},
            .indexDataOffset = 0,
            .transformData = {},
            .transformDataOffset = 0,
        };

    auto &bottomLevelAs = mBottomLevelAs.emplace_back();
    bottomLevelAs = context.device.createAccelerationStructure(
          KDGpu::AccelerationStructureOptions{
              .type = KDGpu::AccelerationStructureType::BottomLevel,
              .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
              .geometryTypesAndCount = {
                  {
                      .geometry = triangleGeometry,
                      .maxPrimitiveCount = triangleCount,
                  },
              },
      });

    context.commandRecorder->buildAccelerationStructures({
          .buildGeometryInfos = {
              {
                  .geometries = { triangleGeometry },
                  .destinationStructure = bottomLevelAs,
                  .buildRangeInfos = {
                      {
                          .primitiveCount = triangleCount,
                          .primitiveOffset = 0,
                          .firstVertex = 0,
                          .transformOffset = 0,
                      },
                  },
              },
          },
      });
}

void VKAccelerationStructure::createTopLevelAsInstances(VKContext &context)
{
    // TODO:
    const auto geometryInstance = KDGpu::AccelerationStructureGeometryInstancesData{
        .data = {
            KDGpu::AccelerationStructureGeometryInstance{
                .transform = {
                    { 1.0f, 0.0f, 0.0f, 0.0f },
                    { 0.0f, 1.0f, 0.0f, 0.0f },
                    { 0.0f, 0.0f, 1.0f, 0.0f }
                },
                .instanceCustomIndex = 0,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags = KDGpu::GeometryInstanceFlagBits::TriangleFacingCullDisable,
 //               .accelerationStructure = mBottomLevelAs,
            },
        },
    };

    mTopLevelAs = context.device.createAccelerationStructure({
        .type = KDGpu::AccelerationStructureType::TopLevel,
        .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
        .geometryTypesAndCount = {
            {
                .geometry = geometryInstance,
                .maxPrimitiveCount = 1,
            },
        },
    });

    context.commandRecorder->buildAccelerationStructures({
        .buildGeometryInfos = {
            {
                .geometries = { geometryInstance },
                .destinationStructure = mTopLevelAs,
                .buildRangeInfos = {
                    {
                        .primitiveCount = 1, // 1 BLAS
                        .primitiveOffset = 0,
                        .firstVertex = 0,
                        .transformOffset = 0,
                    },
                },
            },
        },
    });
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

    for (const auto &instance : mInstances) {
        switch (instance.geometryType) {
        case Instance::GeometryType::AxisAlignedBoundingBoxes:
            createBottomLevelAsAabbs(context, instance);
            break;
        case Instance::GeometryType::Triangles:
            createBottomLevelAsTriangles(context, instance);
            break;
        }
    }
    memoryBarrier(*context.commandRecorder);

    createTopLevelAsInstances(context);
}
