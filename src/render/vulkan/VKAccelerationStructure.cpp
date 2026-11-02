#include "VKAccelerationStructure.h"

VKAccelerationStructure::VKAccelerationStructure(
    const AccelerationStructure &accelStruct)
    : mItemId(accelStruct.id)
{
    mUsedItems += accelStruct.id;
    for (auto item : accelStruct.items)
        if (auto instance = castItem<Instance>(item))
            mInstances.push_back({
                .itemId = instance->id,
                .geometrySetId = instance->geometrySetId,
                .transform = instance->transform,
            });
}

bool VKAccelerationStructure::operator==(
    const VKAccelerationStructure &rhs) const
{
    if (mInstances != rhs.mInstances)
        return false;
    for (auto i = 0u; i < mInstances.size(); ++i) {
        const auto lhsSet = mInstances[i].geometrySet;
        const auto rhsSet = rhs.mInstances[i].geometrySet;
        if (static_cast<bool>(lhsSet) != static_cast<bool>(rhsSet)
            || (lhsSet && *lhsSet != *rhsSet))
            return false;
    }
    return true;
}

void VKAccelerationStructure::setGeometrySet(
    int instanceIndex, VKGeometrySet *geometrySet)
{
    Q_ASSERT(instanceIndex >= 0
        && instanceIndex < static_cast<int>(mInstances.size()));
    mInstances[instanceIndex].geometrySet = geometrySet;
}

void VKAccelerationStructure::memoryBarrier(
    KDGpu::CommandRecorder &commandRecorder,
    KDGpu::PipelineStageFlags destinationStages)
{
    commandRecorder.memoryBarrier(KDGpu::MemoryBarrierOptions{
        .srcStages = KDGpu::PipelineStageFlagBit::AccelerationStructureBuildBit,
        .dstStages = destinationStages,
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
            return ScriptValueList{ };
        auto values = scriptEngine.evaluateValues(transform, itemId);
        if (values.size() != 12 && values.size() != 16) {
            mMessages.insert(itemId, MessageType::UniformComponentMismatch,
                QString("(%1/12 or 16)").arg(values.count()));
            return ScriptValueList{ };
        }
        values.resize(12);
        return values;
    };

    auto geometryInstances =
        KDGpu::AccelerationStructureGeometryInstancesData{ };
    auto instanceIndex = 0u;
    for (const auto &instance : mInstances) {
        mUsedItems += instance.itemId;
        if (!instance.geometrySet)
            return;
        instance.geometrySet->prepare(context, scriptEngine);
        const auto &bottomLevelAs = instance.geometrySet->bottomLevelAs();
        if (!bottomLevelAs.isValid())
            return;
        mUsedItems += instance.geometrySet->usedItems();

        auto &geometryInstance = geometryInstances.data.emplace_back(
            KDGpu::AccelerationStructureGeometryInstance{
                .transform = { {
                    { { 1.0f, 0.0f, 0.0f, 0.0f } },
                    { { 0.0f, 1.0f, 0.0f, 0.0f } },
                    { { 0.0f, 0.0f, 1.0f, 0.0f } },
                } },
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

    if (geometryInstances.data.empty())
        return;
    mTopLevelAs = context.device.createAccelerationStructure({
        .type = KDGpu::AccelerationStructureType::TopLevel,
        .flags = KDGpu::AccelerationStructureFlagBits::PreferFastTrace,
        .geometryTypesAndCount = {
            {
                .geometry = geometryInstances,
                .maxPrimitiveCount =
                    static_cast<uint32_t>(geometryInstances.data.size()),
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
                        .primitiveCount = static_cast<uint32_t>(
                            geometryInstances.data.size()),
                        .primitiveOffset = 0,
                        .firstVertex = 0,
                        .transformOffset = 0,
                    },
                },
            },
        },
    });
    memoryBarrier(*context.commandRecorder,
        KDGpu::PipelineStageFlagBit::AllCommandsBit);
}
