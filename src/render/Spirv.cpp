
#include "Spirv.h"
#include <QRegularExpression>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/resource_limits_c.h>

namespace 
{
    void staticInitGlslang()
    {
        static struct Init {
            Init() {
                glslang::InitializeProcess();
            }
            ~Init() {
                glslang::FinalizeProcess();
            }
        } s;
    }

    glslang_source_t getLanguage(Shader::Language language)
    {
        switch (language) {
            case Shader::Language::GLSL: return GLSLANG_SOURCE_GLSL;
            case Shader::Language::HLSL: return GLSLANG_SOURCE_HLSL;
        }
        return { };
    }

    glslang_stage_t getStage(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
            case Shader::ShaderType::Vertex: return GLSLANG_STAGE_VERTEX;
            case Shader::ShaderType::Fragment: return GLSLANG_STAGE_FRAGMENT;
            case Shader::ShaderType::Geometry: return GLSLANG_STAGE_GEOMETRY;
            case Shader::ShaderType::TessellationControl: return GLSLANG_STAGE_TESSCONTROL;
            case Shader::ShaderType::TessellationEvaluation: return GLSLANG_STAGE_TESSEVALUATION;
            case Shader::ShaderType::Compute: return GLSLANG_STAGE_COMPUTE;
        }
        return { };
    }

    bool parseGLSLangErrors(const QString &log,
          MessagePtrSet &messages, ItemId itemId,
          const QStringList &fileNames)
    {
        // ERROR: 0:50: 'output' : unknown variable
        // WARNING: Linking fragment stage
        static const auto split = QRegularExpression(
            "("
                "([^:]+):\\s*" // 2. severity/code
                "((\\d+):"     // 4. source index
                "(\\d+):)?"    // 5. line
                "\\s*"
            ")?"
            "([^\\n]+)");  // 6. text

        auto hasErrors = false;
        for (auto matches = split.globalMatch(log); matches.hasNext(); ) {
            auto match = matches.next();
            if (match.captured(2).isEmpty())
                continue;

            const auto severity = match.captured(2);
            const auto sourceIndex = match.captured(4).toInt();
            const auto lineNumber = match.captured(5).toInt();
            const auto text = match.captured(6);

            if (text.contains("No code generated") || 
                text.contains("compilation terminated"))
                continue;

            auto messageType = MessageType::ShaderWarning;
            if (severity.contains("INFO", Qt::CaseInsensitive))
                messageType = MessageType::ShaderInfo;
            if (severity.contains("ERROR", Qt::CaseInsensitive)) {
                messageType = MessageType::ShaderError;
                hasErrors = true;
            }

            if (sourceIndex < fileNames.size()) {
                messages += MessageList::insert(
                    fileNames[sourceIndex], lineNumber, messageType, text);
            }
            else {
                messages += MessageList::insert(
                    itemId, messageType, text);
            }
        }
        return hasErrors;
    }
} // namespace

Spirv::Interface::Interface(const std::vector<uint32_t>& spirv)
{
    auto module = SpvReflectShaderModule{ };
    [[maybe_unused]] const auto result = spvReflectCreateShaderModule(
        spirv.size() * sizeof(uint32_t), spirv.data(), &module);
    Q_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    mModule = std::shared_ptr<SpvReflectShaderModule>(
        new SpvReflectShaderModule(module), spvReflectDestroyShaderModule);
}

Spirv::Interface::~Interface() = default;

Spirv::Interface::operator bool() const
{
    return static_cast<bool>(mModule);
}

const SpvReflectShaderModule* Spirv::Interface::operator->() const
{
    Q_ASSERT(mModule);
    return mModule.get();
}

//-------------------------------------------------------------------------

Spirv Spirv::generate(Shader::Language shaderLanguage, 
    Shader::ShaderType shaderType, const QStringList &sources, 
    const QStringList &fileNames, const QString &entryPoint, 
    MessagePtrSet &messages)
{
    staticInitGlslang();

    const auto itemId = 0;
    const auto language = getLanguage(shaderLanguage);
    const auto stage = getStage(shaderType);
    auto program = glslang_program_create();
    auto shaders = std::vector<glslang_shader_t*>();
    auto cleanup = qScopeGuard([&]() {
        glslang_program_delete(program);
        for (auto shader : shaders)
            glslang_shader_delete(shader);            
    });

    for (auto i = 0; i < sources.size(); ++i) {
        const auto source = sources[i].toUtf8();

        const auto input = glslang_input_t{
            .language = language,
            .stage = stage,
            .client = GLSLANG_CLIENT_VULKAN,
            .client_version = GLSLANG_TARGET_VULKAN_1_2,
            .target_language = GLSLANG_TARGET_SPV,
            .target_language_version = GLSLANG_TARGET_SPV_1_5,
            .code = source.constData(),
            .default_version = 100,
            .default_profile = GLSLANG_NO_PROFILE,
            .force_default_version_and_profile = false,
            .forward_compatible = false,
            .messages = GLSLANG_MSG_DEFAULT_BIT,
            .resource = glslang_default_resource(),
        };

        auto shader = glslang_shader_create(&input);
        glslang_shader_set_options(shader, 
            GLSLANG_SHADER_AUTO_MAP_BINDINGS | 
            GLSLANG_SHADER_AUTO_MAP_LOCATIONS | 
            GLSLANG_SHADER_VULKAN_RULES_RELAXED);

        shaders.push_back(shader);

        if (!glslang_shader_preprocess(shader, &input) ||
            !glslang_shader_parse(shader, &input))	{

            parseGLSLangErrors(QString::fromUtf8(glslang_shader_get_info_log(shader)), 
                messages, itemId, fileNames);
            return { };
        }
        glslang_program_add_shader(program, shader);
    }

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        parseGLSLangErrors(QString::fromUtf8(glslang_program_get_info_log(program)), 
            messages, itemId, fileNames);
        return { };
    }
    
    glslang_program_SPIRV_generate(program, stage);
    const auto size = glslang_program_SPIRV_get_size(program);
    auto result = std::vector<uint32_t>(size);
    glslang_program_SPIRV_get(program, result.data());

    if (const char* log = glslang_program_SPIRV_get_messages(program)) {
        parseGLSLangErrors(QString::fromUtf8(log), messages, itemId, fileNames);
    }
    return Spirv(std::move(result));
}

Spirv::Spirv(std::vector<uint32_t> spirv) 
    : mSpirv(std::move(spirv))
{
}

Spirv::Interface Spirv::getInterface() const
{
    return Spirv::Interface(mSpirv);
}
