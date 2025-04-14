
#include "VKAccelerationStructure.h"

VKAccelerationStructure::VKAccelerationStructure(
    const AccelerationStructure &accelStruct)
    : mItemId(accelStruct.id)
{
}

bool VKAccelerationStructure::operator==(
    const VKAccelerationStructure &rhs) const
{
    // TODO:
    return false;
}

void VKAccelerationStructure::prepare(VKContext &context) {
}
