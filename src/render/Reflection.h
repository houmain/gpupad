#pragma once

#include "../session/Item.h"
#include "spirv-reflect/spirv_reflect.h"
#include <memory>

class Reflection
{
public:
    Reflection() = default;
    explicit Reflection(const std::vector<uint32_t> &spirv);
    ~Reflection();

    explicit operator bool() const;
    const SpvReflectShaderModule &operator*() const;
    const SpvReflectShaderModule *operator->() const;

private:
    std::shared_ptr<const SpvReflectShaderModule> mModule;
};

constexpr const char globalUniformBlockName[]{ "$Global" };
bool isGlobalUniformBlockName(const char *name);
bool isGlobalUniformBlockName(const QString &name);
QString removeGlobalUniformBlockName(QString string);

QString getJsonString(const SpvReflectShaderModule &module);
bool isBuiltIn(const SpvReflectInterfaceVariable &variable);
uint32_t getBindingArraySize(const SpvReflectBindingArrayTraits &array);
Field::DataType getBufferMemberDataType(
    const SpvReflectBlockVariable &variable);
int getBufferMemberColumnCount(const SpvReflectBlockVariable &variable);
int getBufferMemberRowCount(const SpvReflectBlockVariable &variable);
int getBufferMemberColumnStride(const SpvReflectBlockVariable &variable);
int getBufferMemberArraySize(const SpvReflectBlockVariable &variable);
int getBufferMemberArrayStride(const SpvReflectBlockVariable &variable);
GLenum getBufferMemberGLType(const SpvReflectBlockVariable &variable);
