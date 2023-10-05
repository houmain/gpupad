
#include "glslang.h"
#include <QProcess>
#include <QDir>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <cstring>

#if defined(SPIRV_CROSS_ENABLED)
#  include "spirv_glsl.hpp"
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
            messages += MessageList::insert(0, MessageType::GlslangValidatorNotFound);
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
            const auto line = match.captured(5).toInt();
            const auto text = match.captured(6);

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

    QString patchGLSL(QString glsl) 
    {
        // WORKAROUND - is there no simpler way?
        // turn:
        //   layout(binding = 0, std140) uniform _Global
        //   {
        //       layout(row_major) mat4 viewProj;
        //   } _35;
        //   ...
        //   _35.viewProj
        //
        // into:
        //   uniform mat4 viewProj;
        //   ...
        //   viewProj
        //
        auto i = glsl.indexOf("uniform _Global");
        if (i >= 0) {
            auto lineBegin = glsl.lastIndexOf('\n', i) + 1;
            glsl.insert(lineBegin, "//");
            for (auto level = 0; i < glsl.size(); ++i) {
                if (glsl[i] == '{') {
                    if (level == 0) {
                        glsl.insert(i, "//");
                        i += 2;
                    }
                    ++level;
                }
                else if (glsl[i] == 'l' && glsl.indexOf("layout(", i) == i) {
                    lineBegin = i;
                    glsl.remove(i, glsl.indexOf(')', i) - i + 1);
                }
                else if (glsl[i] == '\n') {
                    lineBegin = i + 1;
                }
                else if (glsl[i] == ';') {
                    glsl.insert(lineBegin, "uniform ");
                    i += 8;
                }
                else if (glsl[i] == '}') {
                    --level;
                    if (level == 0) {
                        glsl.insert(i, "//");
                        i += 4;
                        const auto var = glsl.mid(i, glsl.indexOf(';', i) - i);
                        glsl.remove(var + ".");
                        break;
                    }
                }
            }
        }
        return glsl;
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
    const QString &fileName, ItemId itemId, MessagePtrSet &messages)
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

            return patchGLSL(QString::fromStdString(compiler.compile()));
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
