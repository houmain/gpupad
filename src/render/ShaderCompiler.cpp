
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
            messages.insert(programItemId, MessageType::ProgramHasNoShader);
            return {};
        }

        if (session.shaderCompiler == Session::ShaderCompiler::DXC) {
#if defined(_WIN32)
            auto stageSpirv = std::map<Shader::ShaderType, Spirv>();
            for (const auto &input : inputs) {
                auto spirv = compileSpirv_DXC(session, { input }, messages);
                if (!spirv.empty())
                    stageSpirv[input.shaderType] = std::move(spirv);
            }
            return stageSpirv;
#else
            messages.insert(programItemId, MessageType::UnsupportedShaderType);
            return {};
#endif
        }
        return compileSpirv_glslang(session, inputs, programItemId, messages);
    }

    Spirv compileSpirvVulkanGLSL(Shader::ShaderType shaderType,
        const QString &source)
    {
        auto session = Session{};
        session.type = Item::Type::Session;
        session.renderer = Session::Renderer::Vulkan;
        session.shaderLanguage = Session::ShaderLanguage::GLSL;
        session.shaderCompiler = Session::ShaderCompiler::glslang;

        const auto input = Input{
            .shaderType = shaderType,
            .sources = { source },
            .fileNames = { "shader" },
            .entryPoint = "main",
        };
        auto messages = MessagePtrSet{};
        const auto shaders =
            compileSpirv_glslang(session, { input }, 0, messages);
        Q_ASSERT(shaders.size() == 1);
        return (!shaders.empty() ? shaders.begin()->second : Spirv{});
    }
} // namespace ShaderCompiler
