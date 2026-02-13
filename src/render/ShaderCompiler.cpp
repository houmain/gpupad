
#include "ShaderCompiler.h"

namespace ShaderCompiler {

    extern std::map<Shader::ShaderType, Spirv> compileSpirv_glslang(
        const Session &session, const std::vector<Input> &inputs,
        ItemId programItemId, MessagePtrSet &messages);

    extern Spirv compileSpirv_DXC(const Session &session, const Input &input,
        MessagePtrSet &messages);

    std::map<Shader::ShaderType, Spirv> compileSpirv(const Session &session,
        const std::vector<Input> &inputs, ItemId programItemId,
        MessagePtrSet &messages)
    {
        if (inputs.empty()) {
            messages += MessageList::insert(programItemId,
                MessageType::ProgramHasNoShader);
            return {};
        }

        if (session.shaderCompiler == Session::ShaderCompiler::glslang)
            return compileSpirv_glslang(session, inputs, programItemId,
                messages);

#if defined(_WIN32)
        if (session.shaderCompiler == Session::ShaderCompiler::DXC) {
            auto stageSpirv = std::map<Shader::ShaderType, Spirv>();
            for (const auto &input : inputs) {
                auto spirv = compileSpirv_DXC(session, { input }, messages);
                if (!spirv.empty())
                    stageSpirv[input.shaderType] = std::move(spirv);
            }
            return stageSpirv;
        }
#endif

        Q_ASSERT(!"unexpected shader compiler");
        return {};
    }
} // namespace ShaderCompiler