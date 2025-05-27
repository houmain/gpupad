
#include "Spirv.h"
#include "ShaderBase.h"
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <sstream>
#include <set>

#define ENABLE_HLSL
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/disassemble.h>

#if __has_include(<glslang/MachineIndependent/LiveTraverser.h>)
#  include <glslang/MachineIndependent/LiveTraverser.h>
#endif

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
        case Shader::ShaderType::Vertex:          return EShLangVertex;
        case Shader::ShaderType::Fragment:        return EShLangFragment;
        case Shader::ShaderType::Geometry:        return EShLangGeometry;
        case Shader::ShaderType::TessControl:     return EShLangTessControl;
        case Shader::ShaderType::TessEvaluation:  return EShLangTessEvaluation;
        case Shader::ShaderType::Compute:         return EShLangCompute;
        case Shader::ShaderType::Task:            return EShLangTask;
        case Shader::ShaderType::Mesh:            return EShLangMesh;
        case Shader::ShaderType::RayGeneration:   return EShLangRayGen;
        case Shader::ShaderType::RayIntersection: return EShLangIntersect;
        case Shader::ShaderType::RayAnyHit:       return EShLangAnyHit;
        case Shader::ShaderType::RayClosestHit:   return EShLangClosestHit;
        case Shader::ShaderType::RayMiss:         return EShLangMiss;
        case Shader::ShaderType::RayCallable:     return EShLangCallable;
        case Shader::ShaderType::Includable:      break;
        }
        return {};
    }

    glslang::EShClient getClient(const QString &renderer)
    {
        return (renderer == "Vulkan" ? glslang::EShClient::EShClientVulkan
                                     : glslang::EShClient::EShClientOpenGL);
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
        Shader::Language language, int dialectVersion,
        Shader::ShaderType shaderType, const QStringList &sources,
        const QStringList &fileNames, const QString &entryPoint)
    {
        staticInitGlslang();

        const auto client = getClient(session.renderer);
        const auto clientVersion = getClientVersion(client, 0);
        const auto targetLanguage = glslang::EShTargetLanguage::EShTargetSpv;
        const auto targetVersion = getSpirvVersion(session.spirvVersion);

        auto lengths = std::vector<int>();
        auto sourcesUtf8 = std::vector<QByteArray>();
        auto sourcesCStr = std::vector<const char *>();
        for (const auto &source : sources) {
            sourcesUtf8.push_back(source.toUtf8());
            sourcesCStr.push_back(sourcesUtf8.back().constData());
            lengths.push_back(static_cast<int>(sourcesUtf8.back().size()));
        }

        auto fileNamesUtf8 = std::vector<QByteArray>();
        auto fileNamesCStr = std::vector<const char *>();
        for (const auto &fileName : fileNames) {
            fileNamesUtf8.push_back(fileName.toUtf8());
            fileNamesCStr.push_back(fileNamesUtf8.back().constData());
        }

        // capture sources and filenames in deleter lambda
        const auto sourcesPtr = sourcesCStr.data();
        const auto fileNamesPtr = fileNamesCStr.data();
        const auto shaderPtr = std::shared_ptr<glslang::TShader>(
            new glslang::TShader(getStage(shaderType)),
            [sourcesUtf8 = std::move(sourcesUtf8),
                sourcesCStr = std::move(sourcesCStr),
                fileNamesUtf8 = std::move(fileNamesUtf8),
                fileNamesCStr = std::move(fileNamesCStr)](
                glslang::TShader *shader) { delete shader; });

        auto &shader = *shaderPtr;
        shader.setStringsWithLengthsAndNames(sourcesPtr, lengths.data(),
            fileNamesPtr, static_cast<int>(sources.size()));
        shader.setEnvInput(getLanguage(language), getStage(shaderType), client,
            dialectVersion);
        shader.setEnvClient(client, clientVersion);
        shader.setEnvTarget(targetLanguage, targetVersion);

        const auto entryPointU8 = entryPoint.toUtf8();
        shader.setEntryPoint(entryPointU8.constData());
        shader.setSourceEntryPoint(entryPointU8.constData());

        shader.setAutoMapBindings(session.autoMapBindings);
        shader.setAutoMapLocations(session.autoMapLocations);
        if (session.vulkanRulesRelaxed) {
            shader.setEnvInputVulkanRulesRelaxed();
            shader.setGlobalUniformBlockName(globalUniformBlockName);
        }
        shader.setHlslIoMapping(language == Shader::Language::HLSL);
        if (session.autoSampledTextures)
            shader.setTextureSamplerTransformMode(
                EShTexSampTransUpgradeTextureRemoveSampler);
        return shaderPtr;
    }

    void patchHLSLGlobalUniformBindingSet(glslang::TShader &shader)
    {
#if __has_include(<glslang/MachineIndependent/LiveTraverser.h>)
        struct Traverser : glslang::TLiveTraverser
        {
            using glslang::TLiveTraverser::TLiveTraverser;

            void visitSymbol(glslang::TIntermSymbol *base) override
            {
                if (base->getQualifier().isUniformOrBuffer()
                    && isGlobalUniformBlockName(
                        base->getAccessName().c_str())) {
                    auto &qualifier = base->getWritableType().getQualifier();
                    // TODO: simply set to something reasonable high for now
                    qualifier.layoutSet = 4;
                    qualifier.layoutBinding = 31;
                }
            }
        };
        if (auto intermediate = shader.getIntermediate())
            if (auto root = intermediate->getTreeRoot()) {
                Traverser traverser(*intermediate, false, false, false, true);
                root->traverse(&traverser);
            }
#endif
    }

    class Includer : public glslang::TShader::Includer
    {
    private:
        const QString mIncludePaths;

        class IncludeResultWithData : public IncludeResult
        {
        private:
            QByteArray mData;

        public:
            IncludeResultWithData(const std::string &headerName,
                QByteArray data)
                : IncludeResult(headerName, data.constData(), data.size(),
                      nullptr)
                , mData(data)
            {
            }
        };

    public:
        explicit Includer(const QString &includePaths)
            : mIncludePaths(includePaths)
        {
        }

        IncludeResult *includeSystem(const char *headerName,
            const char *includerName, size_t inclusionDepth) override
        {
            if (inclusionDepth > 3)
                return nullptr;

            const auto fileName =
                resolveIncludePath(includerName, headerName, mIncludePaths);

            auto file = QFile(fileName);
            if (!file.open(QFile::ReadOnly | QFile::Text))
                return nullptr;

            return new IncludeResultWithData(fileName.toStdString(),
                QTextStream(&file).readAll().toUtf8());
        }
        void releaseInclude(IncludeResult *result) override { delete result; }
    };
} // namespace

bool isGlobalUniformBlockName(const char *name)
{
    return (name && !std::strcmp(name, globalUniformBlockName));
}

bool isGlobalUniformBlockName(QStringView name)
{
    const auto size = sizeof(globalUniformBlockName) - 1;
    if (name.size() != size)
        return false;
    for (auto i = 0u; i < size; ++i)
        if (name[i] != globalUniformBlockName[i])
            return false;
    return true;
}

QString removeGlobalUniformBlockName(QString string)
{
    if (string.startsWith(globalUniformBlockName))
        return string.mid(sizeof(globalUniformBlockName));
    return string;
}

//-------------------------------------------------------------------------

Spirv::Spirv(std::vector<uint32_t> spirv) : mSpirv(std::move(spirv)) { }

Spirv::Interface Spirv::getInterface() const
{
    return Spirv::Interface(mSpirv);
}

//-------------------------------------------------------------------------

std::map<Shader::ShaderType, Spirv> Spirv::compile(const Session &session,
    const std::vector<Input> &inputs, ItemId programItemId,
    MessagePtrSet &messages)
{
    if (inputs.empty()) {
        messages +=
            MessageList::insert(programItemId, MessageType::ProgramHasNoShader);
        return {};
    }

    const auto defaultVersion = 100;
    const auto defaultProfile = ECoreProfile;
    const auto forwardCompatible = true;

    auto shaderTypes = std::set<Shader::ShaderType>();
    auto shaders = std::vector<std::shared_ptr<glslang::TShader>>();
    auto program = glslang::TProgram();
    for (const auto &input : inputs) {
        auto requestedMessages = unsigned{ EShMsgSpvRules };
        if (session.renderer == "Vulkan")
            requestedMessages |= EShMsgVulkanRules;
        if (input.language == Shader::Language::HLSL)
            requestedMessages |= EShMsgReadHlsl | EShMsgHlslOffsets;

        auto &shader = *shaders.emplace_back(createShader(session,
            input.language, defaultVersion, input.shaderType, input.sources,
            input.fileNames, input.entryPoint));

        auto includer = Includer(input.includePaths);
        if (!shader.parse(GetDefaultResources(), defaultVersion, defaultProfile,
                false, forwardCompatible,
                static_cast<EShMessages>(requestedMessages), includer)) {
            parseGLSLangErrors(QString::fromUtf8(shader.getInfoLog()), messages,
                input.itemId, input.fileNames);
            return {};
        }

        // setGlobalUniformBinding and setGlobalUniformSet do not work
        if (input.language == Shader::Language::HLSL)
            patchHLSLGlobalUniformBindingSet(shader);

        program.addShader(&shader);
        shaderTypes.insert(input.shaderType);
    }

    if (!program.link(EShMessages::EShMsgDefault)) {
        parseGLSLangErrors(QString::fromUtf8(program.getInfoLog()), messages,
            programItemId, {});
        return {};
    }

    // do not auto map locations/bindings when targeting OpenGL
    // since it starts counting from 0 in each stage
    if (session.renderer == "Vulkan")
        if (!program.mapIO()) {
            messages += MessageList::insert(programItemId,
                MessageType::ShaderError, "mapping program IO failed");
            return {};
        }

    auto spvOptions = glslang::SpvOptions{
        .generateDebugInfo = true,
        .disableOptimizer = false,
        .optimizeSize = false,
    };

    auto stages = std::map<Shader::ShaderType, Spirv>();
    for (auto shaderType : shaderTypes) {
        auto spirv = std::vector<uint32_t>();
        glslang::GlslangToSpv(*program.getIntermediate(getStage(shaderType)),
            spirv, &spvOptions);
        Q_ASSERT(!spirv.empty());
        stages.emplace(shaderType, std::move(spirv));
    }
    return stages;
}

QString Spirv::preprocess(const Session &session, Shader::Language language,
    Shader::ShaderType shaderType, const QStringList &sources,
    const QStringList &fileNames, const QString &entryPoint, ItemId itemId,
    const QString &includePaths, MessagePtrSet &messages)
{
    const auto defaultVersion = 100;
    const auto defaultProfile = ENoProfile;
    const auto forwardCompatible = true;
    const auto requestedMessages = EShMsgOnlyPreprocessor;

    auto shader = createShader(session, language, defaultVersion, shaderType,
        sources, fileNames, entryPoint);
    auto string = std::string();
    auto includer = Includer(includePaths);
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
    const QString &includePaths, MessagePtrSet &messages)
{
    const auto defaultVersion = 100;
    const auto defaultProfile = ENoProfile;
    const auto forwardCompatible = true;
    const auto requestedMessages = EShMsgAST;

    auto shader = createShader(session, language, defaultVersion, shaderType,
        sources, fileNames, entryPoint);
    auto includer = Includer(includePaths);
    if (!shader->parse(GetDefaultResources(), defaultVersion, defaultProfile,
            false, forwardCompatible, requestedMessages, includer)) {
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
