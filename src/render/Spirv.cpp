
#include "Spirv.h"
#include <QRegularExpression>

#define ENABLE_HLSL
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/disassemble.h>

#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>

namespace {
    void staticInitGlslang()
    {
        static struct Init
        {
            Init() { glslang::InitializeProcess(); }
            ~Init() { glslang::FinalizeProcess(); }
        } s;
    }

    glslang::EShSource getLanguage(Shader::Language language)
    {
        switch (language) {
        case Shader::Language::GLSL: return glslang::EShSourceGlsl;
        case Shader::Language::HLSL: return glslang::EShSourceHlsl;
        case Shader::Language::None: break;
        }
        return {};
    }

    EShLanguage getStage(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
        case Shader::ShaderType::Vertex:              return EShLangVertex;
        case Shader::ShaderType::Fragment:            return EShLangFragment;
        case Shader::ShaderType::Geometry:            return EShLangGeometry;
        case Shader::ShaderType::TessellationControl: return EShLangTessControl;
        case Shader::ShaderType::TessellationEvaluation:
            return EShLangTessEvaluation;
        case Shader::ShaderType::Compute:    return EShLangCompute;
        case Shader::ShaderType::Includable: break;
        }
        return {};
    }

    glslang::EShClient getClient(const QString &renderer)
    {
        return (renderer == "OpenGL" ? glslang::EShClient::EShClientOpenGL
                                     : glslang::EShClient::EShClientVulkan);
    }

    glslang::EShTargetClientVersion getClientVersion(glslang::EShClient client,
        int version)
    {
        using Version = glslang::EShTargetClientVersion;
        if (client == glslang::EShClient::EShClientOpenGL)
            return Version::EShTargetOpenGL_450;

        Q_ASSERT(client == glslang::EShClient::EShClientVulkan);
        if (!version)
            return Version::EShTargetVulkan_1_3;

        static_assert(Version::EShTargetVulkan_1_3 == (1 << 22) + (3 << 12));
        const auto major = version / 10;
        const auto minor = version % 10;
        return static_cast<Version>((major << 22) + (minor << 12));
    }

    glslang::EShTargetLanguageVersion getSpirvVersion(int version)
    {
        using Version = glslang::EShTargetLanguageVersion;
        if (!version)
            return Version::EShTargetSpv_1_0;

        static_assert(Version::EShTargetSpv_1_6 == (1 << 16) + (6 << 8));
        const auto major = version / 10;
        const auto minor = version % 10;
        return static_cast<Version>((major << 16) + (minor << 8));
    }

    bool parseGLSLangErrors(const QString &log, MessagePtrSet &messages,
        ItemId itemId, const QStringList &fileNames)
    {
        // ERROR: 0:50: 'output' : unknown variable
        // WARNING: Linking fragment stage
        static const auto split = QRegularExpression(
            "("
            "([^:]+):\\s*" // 2. severity/code
            "((\\d+):" // 4. source index
            "(\\d+):)?" // 5. line
            "\\s*"
            ")?"
            "([^\\n]+)"); // 6. text

        auto hasErrors = false;
        for (auto matches = split.globalMatch(log); matches.hasNext();) {
            auto match = matches.next();
            if (match.captured(2).isEmpty())
                continue;

            const auto severity = match.captured(2);
            const auto sourceIndex = match.captured(4).toInt();
            const auto lineNumber = match.captured(5).toInt();
            const auto text = match.captured(6);

            if (text.contains("No code generated")
                || text.contains("compilation terminated"))
                continue;

            auto messageType = MessageType::ShaderWarning;
            if (severity.contains("INFO", Qt::CaseInsensitive))
                messageType = MessageType::ShaderInfo;
            if (severity.contains("ERROR", Qt::CaseInsensitive)) {
                messageType = MessageType::ShaderError;
                hasErrors = true;
            }

            if (sourceIndex < fileNames.size()) {
                messages += MessageList::insert(fileNames[sourceIndex],
                    lineNumber, messageType, text);
            } else {
                messages += MessageList::insert(itemId, messageType, text);
            }
        }
        return hasErrors;
    }

    std::shared_ptr<glslang::TShader> createShader(const Session &session,
        Shader::Language language, Shader::ShaderType shaderType,
        const QStringList &sources, const QString &entryPoint,
        int shiftBindingsInSet0 = 0)
    {
        staticInitGlslang();

        const auto client = getClient(session.renderer);
        const auto clientVersion = getClientVersion(client, 0);
        const auto targetLanguage = glslang::EShTargetLanguage::EShTargetSpv;
        const auto targetVersion = getSpirvVersion(session.spirvVersion);

        // capture sources in deleter lambda
        auto sourcesUtf8 = std::vector<QByteArray>();
        auto sourcesCStr = std::vector<const char *>();
        for (const auto &source : sources) {
            sourcesUtf8.push_back(source.toUtf8());
            sourcesCStr.push_back(sourcesUtf8.back().constData());
        }
        auto sourcesPtr = sourcesCStr.data();
        auto shaderPtr = std::shared_ptr<glslang::TShader>(
            new glslang::TShader(getStage(shaderType)),
            [sourcesUtf8 = std::move(sourcesUtf8),
                sourcesCStr = std::move(sourcesCStr)](
                glslang::TShader *shader) { delete shader; });

        auto &shader = *shaderPtr;
        shader.setStrings(sourcesPtr, static_cast<int>(sources.size()));
        shader.setEnvInput(getLanguage(language), getStage(shaderType), client,
            clientVersion);
        shader.setEnvClient(client, clientVersion);
        shader.setEnvTarget(targetLanguage, targetVersion);

        const auto entryPointU8 = entryPoint.toUtf8();
        shader.setEntryPoint(entryPointU8.constData());
        shader.setSourceEntryPoint(entryPointU8.constData());

        if (session.autoMapBindings)
            shader.setAutoMapBindings(true);
        if (session.autoMapLocations)
            shader.setAutoMapLocations(true);
        if (session.vulkanRulesRelaxed)
            shader.setEnvInputVulkanRulesRelaxed();
        shader.setHlslIoMapping(language == Shader::Language::HLSL);

        using ResTypes = glslang::TResourceType;
        for (auto e = ResTypes{}; e != ResTypes::EResCount;
             e = static_cast<ResTypes>(e + 1))
            shader.setShiftBindingForSet(e, shiftBindingsInSet0, 0);

        return shaderPtr;
    }
} // namespace

Spirv::Interface::Interface(const std::vector<uint32_t> &spirv)
{
    auto module = SpvReflectShaderModule{};
    const auto result = spvReflectCreateShaderModule(
        spirv.size() * sizeof(uint32_t), spirv.data(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
        module = {};
    mModule = std::shared_ptr<SpvReflectShaderModule>(
        new SpvReflectShaderModule(module), spvReflectDestroyShaderModule);
}

Spirv::Interface::~Interface() = default;

Spirv::Interface::operator bool() const
{
    return static_cast<bool>(mModule);
}

const SpvReflectShaderModule *Spirv::Interface::operator->() const
{
    Q_ASSERT(mModule);
    return mModule.get();
}

//-------------------------------------------------------------------------

Spirv::Spirv(std::vector<uint32_t> spirv) : mSpirv(std::move(spirv)) { }

Spirv::Interface Spirv::getInterface() const
{
    return Spirv::Interface(mSpirv);
}

//-------------------------------------------------------------------------

Spirv Spirv::generate(const Session &session, Shader::Language language,
    Shader::ShaderType shaderType, const QStringList &sources,
    const QStringList &fileNames, const QString &entryPoint,
    int shiftBindingsInSet0, ItemId itemId, MessagePtrSet &messages)
{
    if (sources.empty() || sources.size() != fileNames.size())
        return {};

    const auto defaultVersion = 100;
    const auto defaultProfile = ENoProfile;
    const auto forwardCompatible = true;

    auto requestedMessages = unsigned{ EShMsgSpvRules };
    if (session.renderer == "Vulkan")
        requestedMessages |= EShMsgVulkanRules;
    if (language == Shader::Language::HLSL)
        requestedMessages |= EShMsgReadHlsl | EShMsgHlslOffsets;

    auto shader = createShader(session, language, shaderType, sources,
        entryPoint, shiftBindingsInSet0);

    if (!shader->parse(GetDefaultResources(), defaultVersion, defaultProfile,
            false, forwardCompatible,
            static_cast<EShMessages>(requestedMessages))) {
        parseGLSLangErrors(QString::fromUtf8(shader->getInfoLog()), messages,
            itemId, fileNames);
        return {};
    }

    auto program = glslang::TProgram();
    program.addShader(shader.get());
    if (!program.link(static_cast<EShMessages>(requestedMessages))) {
        parseGLSLangErrors(QString::fromUtf8(program.getInfoLog()), messages,
            itemId, fileNames);
        return {};
    }

    program.mapIO();

    auto spvOptions = glslang::SpvOptions{
        .generateDebugInfo = true,
        .disableOptimizer = false,
        .optimizeSize = false,
    };

    auto spirv = std::vector<uint32_t>();
    glslang::GlslangToSpv(*program.getIntermediate(getStage(shaderType)), spirv,
        &spvOptions);
    Q_ASSERT(!spirv.empty());

    return Spirv(std::move(spirv));
}

QString Spirv::preprocess(const Session &session, Shader::Language language,
    Shader::ShaderType shaderType, const QStringList &sources,
    const QStringList &fileNames, const QString &entryPoint, ItemId itemId,
    MessagePtrSet &messages)
{
    if (sources.empty() || sources.size() != fileNames.size())
        return {};

    const auto defaultVersion = 100;
    const auto defaultProfile = ENoProfile;
    const auto forwardCompatible = true;
    const auto requestedMessages = EShMsgOnlyPreprocessor;

    auto shader =
        createShader(session, language, shaderType, sources, entryPoint);
    auto string = std::string();
    auto includer = glslang::TShader::ForbidIncluder();
    if (!shader->preprocess(GetResources(), defaultVersion, defaultProfile,
            false, forwardCompatible, requestedMessages, &string, includer)) {
        parseGLSLangErrors(QString::fromUtf8(shader->getInfoLog()), messages,
            itemId, fileNames);
        return {};
    }
    return QString::fromUtf8(string);
}

QString Spirv::disassemble(const Spirv &spirv)
{
    if (!spirv)
        return {};

    auto ss = std::ostringstream();
    spv::Disassemble(ss, spirv.spirv());
    return QString::fromStdString(ss.str());
}

QString Spirv::generateAST(const Session &session, Shader::Language language,
    Shader::ShaderType shaderType, const QStringList &sources,
    const QStringList &fileNames, const QString &entryPoint, ItemId itemId,
    MessagePtrSet &messages)
{
    if (sources.empty() || sources.size() != fileNames.size())
        return {};

    const auto defaultVersion = 100;
    const auto defaultProfile = ENoProfile;
    const auto forwardCompatible = true;
    const auto requestedMessages = EShMsgAST;

    auto shader =
        createShader(session, language, shaderType, sources, entryPoint);
    if (!shader->parse(GetDefaultResources(), defaultVersion, defaultProfile,
            false, forwardCompatible, requestedMessages)) {
        parseGLSLangErrors(QString::fromUtf8(shader->getInfoLog()), messages,
            itemId, fileNames);
        return {};
    }

    auto program = glslang::TProgram();
    program.addShader(shader.get());
    if (!program.link(requestedMessages)) {
        parseGLSLangErrors(QString::fromUtf8(program.getInfoLog()), messages,
            itemId, fileNames);
        return {};
    }
    return program.getInfoDebugLog();
}
