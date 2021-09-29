
#include "glslang.h"
#include <QProcess>
#include <QDir>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <cstring>

#if defined(SPIRV_CROSS_ENABLED)
#  include "SPIRV-Cross/spirv_glsl.hpp"
#endif

namespace glslang 
{
namespace 
{
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

    QString executeGLSLangValidator(const QString &source, 
        QStringList args, Shader::ShaderType shaderType)
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
        if (!process.waitForStarted())
            return "glslangValidator not found";
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

#if defined(SPIRV_CROSS_ENABLED)
    spv::ExecutionModel getExecutionModel(Shader::ShaderType shaderType)
    {
        switch (shaderType) {
            case Shader::ShaderType::Vertex: return spv::ExecutionModelVertex;
            case Shader::ShaderType::Fragment: return spv::ExecutionModelFragment;
            case Shader::ShaderType::Geometry: return spv::ExecutionModelGeometry;
            case Shader::ShaderType::TessellationControl: return spv::ExecutionModelTessellationControl;
            case Shader::ShaderType::TessellationEvaluation: return spv::ExecutionModelTessellationEvaluation;
            case Shader::ShaderType::Compute: return spv::ExecutionModelGLCompute;
            case Shader::ShaderType::Includable: break;
        }
        return { };
    }
#endif

    bool parseGLSLangErrors(const QString &log, const QString &fileName,
        ItemId itemId, MessagePtrSet &messages)
    {
        // ERROR: 0:50: 'output' : unknown variable
        static const auto split = QRegularExpression(
            "("
                "([^:]+):\\s*" // 2. severity/code
                "(\\d+):"      // 3. source index
                "(\\d+):"      // 4. line
                "\\s*"
            ")?"
            "([^\\n]+)");  // 5. text

        auto hasErrors = false;
        for (auto matches = split.globalMatch(log); matches.hasNext(); ) {
            auto match = matches.next();
            if (match.captured(2).isEmpty())
                continue;

            const auto severity = match.captured(2);
            const auto line = match.captured(4).toInt();
            const auto text = match.captured(5);

            auto messageType = MessageType::ShaderWarning;
            if (severity.contains("INFO", Qt::CaseInsensitive))
                messageType = MessageType::ShaderInfo;
            if (severity.contains("ERROR", Qt::CaseInsensitive)) {
                messageType = MessageType::ShaderError;
                hasErrors = true;
            }

            messages += MessageList::insert(
                fileName, line, messageType, text);
        }
        return hasErrors;
    }
} // namespace

QString generateSpirV(const QString &source, Shader::ShaderType shaderType)
{
    const auto args = QStringList{
      "-H", "--aml", "--amb",
      "--client", "opengl100",
    };
    return executeGLSLangValidator(source, args, shaderType);
}

QString generateAST(const QString &source, Shader::ShaderType shaderType)
{
    const auto args = QStringList{
      "-i", "--aml", "--amb",
      "--client", "opengl100",
    };
    return executeGLSLangValidator(source, args, shaderType);
}

QString preprocess(const QString &source)
{
    const auto args = QStringList{ "-E" };
    return executeGLSLangValidator(source, args, Shader::ShaderType::Vertex);
}

QString generateGLSL(const QString &source, Shader::ShaderType shaderType,
    Shader::Language language, const QString &entryPoint, 
    const QString &fileName, ItemId itemId, MessagePtrSet &messages)
{
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
    args += entryPoint;
    args += "-o";
    args += output.fileName();

    const auto log = executeGLSLangValidator(source, args, shaderType).trimmed();
    if (!log.isEmpty() && parseGLSLangErrors(log, fileName, itemId, messages))
        return { };

#if defined(SPIRV_CROSS_ENABLED)
    output.open();
    auto spirv = output.readAll();
    if (!spirv.isEmpty()) {
        try {
            auto data = std::vector<uint32_t>(spirv.size() / sizeof(uint32_t), 0);
            std::memcpy(data.data(), spirv.data(), data.size() * sizeof(uint32_t));

            auto compiler = spirv_cross::CompilerGLSL(std::move(data));
            auto options = spirv_cross::CompilerGLSL::Options{ };
            options.emit_line_directives = true;
            compiler.set_common_options(options);

            compiler.build_combined_image_samplers();
            for (auto &remap : compiler.get_combined_image_samplers())
               compiler.set_name(remap.combined_id,
                  compiler.get_name(remap.image_id) + "_" +
                        compiler.get_name(remap.sampler_id));

            return QString::fromStdString(compiler.compile());
        }
        catch (const std::exception &ex) {
            messages += MessageList::insert(fileName, -1,
                MessageType::ShaderError, ex.what());
        }
    }
#else
    messages += MessageList::insert(itemId, MessageType::SpirvCrossNotCompiledIn);
#endif

    return { };
}

} // namespace
