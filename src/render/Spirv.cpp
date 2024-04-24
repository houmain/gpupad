
#include "Spirv.h"
#include <QRegularExpression>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

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

    glslang::EShSource getLanguage(Shader::Language language)
    {
        switch (language) {
            case Shader::Language::GLSL: return glslang::EShSourceGlsl;
            case Shader::Language::HLSL: return glslang::EShSourceHlsl;
        }
        return { };
    }

    EShLanguage getStage(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
            case Shader::ShaderType::Vertex: return EShLangVertex;
            case Shader::ShaderType::Fragment: return EShLangFragment;
            case Shader::ShaderType::Geometry: return EShLangGeometry;
            case Shader::ShaderType::TessellationControl: return EShLangTessControl;
            case Shader::ShaderType::TessellationEvaluation: return EShLangTessEvaluation;
            case Shader::ShaderType::Compute: return EShLangCompute;
            case Shader::ShaderType::Includable: break;
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

Spirv Spirv::generate(Shader::Language language, 
    Shader::ShaderType shaderType, const QStringList &sources, 
    const QStringList &fileNames, const QString &entryPoint, 
    MessagePtrSet &messages)
{
    staticInitGlslang();

    const auto itemId = 0;
    const auto client = glslang::EShClient::EShClientVulkan;
    const auto clientVersion = glslang::EShTargetClientVersion::EShTargetVulkan_1_2;
    const auto targetVersion = glslang::EShTargetLanguageVersion::EShTargetSpv_1_4;
    const auto defaultVersion = 100;
    const auto forwardCompatible = false;
    const auto requestedMessages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    auto sourcesUtf8 = std::vector<QByteArray>();
    auto sourcesCStr = std::vector<const char*>();
    for (const auto &source : sources) {
        sourcesUtf8.push_back(source.toUtf8());
        sourcesCStr.push_back(sourcesUtf8.back().constData());
    }
    auto shader = glslang::TShader(getStage(shaderType));
    shader.setStrings(sourcesCStr.data(), static_cast<int>(sources.size()));
    shader.setEnvInput(getLanguage(language), getStage(shaderType), client, clientVersion);
    shader.setEnvClient(client, clientVersion);
    shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, targetVersion);

    // TOOD:
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);
    shader.setEnvInputVulkanRulesRelaxed();

    if (!shader.parse(GetDefaultResources(), defaultVersion, forwardCompatible, requestedMessages)) {
        parseGLSLangErrors(QString::fromUtf8(shader.getInfoLog()), messages, itemId, fileNames);
        return { };
    }

    auto program = glslang::TProgram();
    program.addShader(&shader);
    if (!program.link(requestedMessages)) {
        parseGLSLangErrors(QString::fromUtf8(shader.getInfoLog()), messages, itemId, fileNames);
        return { };
    }

    auto spvOptions = glslang::SpvOptions{ };
    spvOptions.generateDebugInfo = true;
    spvOptions.disableOptimizer = false;
    spvOptions.optimizeSize = false;

    auto spirv = std::vector<uint32_t>();
    glslang::GlslangToSpv(*program.getIntermediate(getStage(shaderType)), 
        spirv, &spvOptions);

    return Spirv(std::move(spirv));
}

Spirv::Spirv(std::vector<uint32_t> spirv) 
    : mSpirv(std::move(spirv))
{
}

Spirv::Interface Spirv::getInterface() const
{
    return Spirv::Interface(mSpirv);
}
