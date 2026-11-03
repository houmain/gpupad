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
                .geometrySetId = (instance->items.empty()
                        ? instance->geometrySetId
                        : instance->id),
                .transform = instance->transform,
                .shaderInstanceId = instance->shaderInstanceId,
                .visibilityMask = instance->visibilityMask,
                .hitGroupRecordOffset = instance->hitGroupRecordOffset,
                .triangleCulling = instance->triangleCulling,
                .frontFace = instance->frontFace,
                .opacityMode = instance->opacityMode,
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
    for (auto instanceIndex = 0u; instanceIndex < mInstances.size();
         ++instanceIndex) {
        const auto &instance = mInstances[instanceIndex];
        mUsedItems += instance.itemId;
        if (!instance.geometrySet)
            return;
        instance.geometrySet->prepare(context, scriptEngine);
        const auto &bottomLevelAs = instance.geometrySet->bottomLevelAs();
        if (!bottomLevelAs.isValid())
            return;
        mUsedItems += instance.geometrySet->usedItems();

        auto shaderInstanceId = static_cast<uint32_t>(instanceIndex);
        if (!instance.shaderInstanceId.isEmpty()) {
            const auto value = scriptEngine.evaluateUInt(
                instance.shaderInstanceId, instance.itemId, 0, 0xFFFFFF,
                QStringLiteral("Shader instance ID"));
            if (!value)
                return;
            shaderInstanceId = *value;
        }
        const auto visibilityMask = scriptEngine.evaluateUInt(
            instance.visibilityMask, instance.itemId, 0, 0xFF,
            QStringLiteral("Visibility mask"));
        if (!visibilityMask)
            return;
        const auto hitGroupRecordOffset = scriptEngine.evaluateUInt(
            instance.hitGroupRecordOffset, instance.itemId, 0, 0xFFFFFF,
            QStringLiteral("Hit group record offset"));
        if (!hitGroupRecordOffset)
            return;

        auto flags = KDGpu::GeometryInstanceFlags{ };
        if (!instance.triangleCulling)
            flags |=
                KDGpu::GeometryInstanceFlagBits::TriangleFacingCullDisable;
        if (instance.frontFace == Instance::FrontFace::CCW)
            flags |= KDGpu::GeometryInstanceFlagBits::TriangleFlipFacing;
        if (instance.opacityMode == Instance::OpacityMode::ForceOpaque)
            flags |= KDGpu::GeometryInstanceFlagBits::ForceOpaque;
        else if (instance.opacityMode
            == Instance::OpacityMode::ForceNonOpaque)
            flags |= KDGpu::GeometryInstanceFlagBits::ForceNoOpaque;

        auto &geometryInstance = geometryInstances.data.emplace_back(
            KDGpu::AccelerationStructureGeometryInstance{
                .transform = { {
                    { { 1.0f, 0.0f, 0.0f, 0.0f } },
                    { { 0.0f, 1.0f, 0.0f, 0.0f } },
                    { { 0.0f, 0.0f, 1.0f, 0.0f } },
                } },
                .instanceCustomIndex = shaderInstanceId,
                .mask = *visibilityMask,
                .instanceShaderBindingTableRecordOffset =
                    *hitGroupRecordOffset,
                .flags = flags,
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
