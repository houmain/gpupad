
#include "glslang.h"
#include "opengl/GLShader.h"
#include <QProcess>
#include <QDir>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QScopeGuard>
#include <cstring>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include "spirvCross.h"

namespace glslang 
{
namespace 
{
    void staticInitGlslang() {
        static struct Init {
            Init() {
                glslang_initialize_process();
            }
            ~Init() {
                glslang_finalize_process();
            }
        } s;
    }

    const char *getGLSLangSourceType(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
            case Shader::ShaderType::Vertex: return "vert";
            case Shader::ShaderType::Fragment: return "frag";
            case Shader::ShaderType::TessellationControl: return "tesc";
            case Shader::ShaderType::TessellationEvaluation: return "tese";
            case Shader::ShaderType::Geometry: return "geom";
            case Shader::ShaderType::Compute: return "comp";

            case Shader::ShaderType::Includable:
                break;
        }
        return nullptr;
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

    QString executeGLSLangValidator(const QString &source,
        QStringList args, Shader::ShaderType shaderType,
        MessagePtrSet &messages)
    {
        args += "--stdin";
        if (auto sourceType = getGLSLangSourceType(shaderType)) {
            args += "-S";
            args += sourceType;
        }
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setWorkingDirectory(QDir::temp().path());
        process.start("glslangValidator", args);
        if (!process.waitForStarted()) {
            return { };
        }
        process.write(source.toUtf8());
        process.closeWriteChannel();

        auto data = QByteArray();
        while (process.waitForReadyRead())
            data.append(process.readAll());

        auto result = QString::fromUtf8(data);
        if (result.startsWith("stdin"))
            return result.mid(5);
        return result;
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

    QString completeFunctionName(const QString &source, const QString& prefix)
    {
        for (auto begin = source.indexOf(prefix); begin > 0; begin = source.indexOf(prefix, begin + 1)) {
            // must be beginning of word
            if (!source[begin - 1].isSpace())
                continue;

            // must not follow struct
            if (source.mid(std::max(static_cast<int>(begin) - 10, 0), 10).contains("struct"))
                continue;

            // complete word
            auto it = begin;
            while (it < source.size() && (source[it].isLetterOrNumber() || source[it] == '_'))
                ++it;

            // ( must follow
            if (it == source.size() || source[it] != '(')
                continue;

            return source.mid(begin, it - begin);
        }
        return { };
    }

    QString findHLSLEntryPoint(Shader::ShaderType shaderType, const QString &source)
    {
        const auto prefix = [&]() {
            switch (shaderType) {
                default:
                case Shader::ShaderType::Fragment: return "PS";
                case Shader::ShaderType::Vertex: return "VS";
                case Shader::ShaderType::Geometry: return "GS";
                case Shader::ShaderType::TessellationControl: return "HS";
                case Shader::ShaderType::TessellationEvaluation: return "DS";
                case Shader::ShaderType::Compute: return "CS";
            }
        }();
        return completeFunctionName(source, prefix);
    }

    QString getEntryPoint(const QString &entryPoint, Shader::Language language, 
        Shader::ShaderType shaderType, const QString &source)
    {
      if (!entryPoint.isEmpty())
          return entryPoint;

      if (language == Shader::Language::HLSL)
          if (auto found = findHLSLEntryPoint(shaderType, source); !found.isEmpty())
              return found;
      
      return "main";
    }
} // namespace

QString generateSpirV(const QString &source, Shader::ShaderType shaderType,
    MessagePtrSet &messages)
{
    const auto args = QStringList{
      "-H", "--aml", "--amb",
      "--client", "opengl100",
    };
    return executeGLSLangValidator(source, args, shaderType, messages);
}

QString generateAST(const QString &source,
    Shader::ShaderType shaderType, MessagePtrSet &messages)
{
    const auto args = QStringList{
      "-i", "--aml", "--amb",
      "--client", "opengl100",
    };
    return executeGLSLangValidator(source, args, shaderType, messages);
}

QString preprocess(const QString &source, MessagePtrSet &messages)
{
    const auto args = QStringList{ "-E" };
    return executeGLSLangValidator(source, args, Shader::ShaderType::Vertex, messages);
}

QString generateGLSL(const QString &source, Shader::ShaderType shaderType,
    Shader::Language language, const QString &entryPoint, 
    const QString &fileName, MessagePtrSet &messages)
{
    Q_ASSERT(language != Shader::Language::None);

    auto output = QTemporaryFile();
    output.open();
    output.close();
    output.setAutoRemove(true);

    auto args = QStringList{
      "-V100", 
      "--aml", "--amb",
      "--keep-uncalled",
      "-Od",
    };
    if (language == Shader::Language::HLSL)
        args += "-D";
    args += "-e";
    args += getEntryPoint(entryPoint, language, shaderType, source);
    args += "-o";
    args += output.fileName();

    const auto log = executeGLSLangValidator(source, args, shaderType, messages).trimmed();
    if (!log.isEmpty() && parseGLSLangErrors(log, messages, 0, { fileName }))
        return { };

    output.open();
    auto spirv = output.readAll();
    if (!spirv.isEmpty()) {
        auto data = std::vector<uint32_t>(spirv.size() / sizeof(uint32_t), 0);
        std::memcpy(data.data(), spirv.data(), data.size() * sizeof(uint32_t));
        return spirvCross::generateGLSL(std::move(data), fileName, messages);
    }

    return { };
}

std::vector<uint32_t> generateSpirVBinary(Shader::Language shaderLanguage, 
        Shader::ShaderType shaderType, const QStringList &sources, 
        const QStringList &fileNames, const QString &entryPoint, 
        MessagePtrSet &messages) {

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
    return result;
}

} // namespace
