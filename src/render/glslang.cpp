
#include "glslang.h"
#include "opengl/GLShader.h"
#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryFile>
#include <cstring>

namespace glslang {
    namespace {
        const char *getGLSLangSourceType(Shader::ShaderType shaderType)
        {
            switch (shaderType) {
            case Shader::ShaderType::Vertex:                 return "vert";
            case Shader::ShaderType::Fragment:               return "frag";
            case Shader::ShaderType::TessellationControl:    return "tesc";
            case Shader::ShaderType::TessellationEvaluation: return "tese";
            case Shader::ShaderType::Geometry:               return "geom";
            case Shader::ShaderType::Compute:                return "comp";

            case Shader::ShaderType::Includable: break;
            }
            return nullptr;
        }

        QString executeGLSLangValidator(const QString &source, QStringList args,
            Shader::ShaderType shaderType, MessagePtrSet &messages)
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
                return {};
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
    } // namespace

    QString generateSpirV(const QString &source, Shader::ShaderType shaderType,
        MessagePtrSet &messages)
    {
        const auto args = QStringList{
            "-H",
            "--aml",
            "--amb",
            "--client",
            "opengl100",
        };
        return executeGLSLangValidator(source, args, shaderType, messages);
    }

    QString generateAST(const QString &source, Shader::ShaderType shaderType,
        MessagePtrSet &messages)
    {
        const auto args = QStringList{
            "-i",
            "--aml",
            "--amb",
            "--client",
            "opengl100",
        };
        return executeGLSLangValidator(source, args, shaderType, messages);
    }

    QString preprocess(const QString &source, MessagePtrSet &messages)
    {
        const auto args = QStringList{ "-E" };
        return executeGLSLangValidator(source, args, Shader::ShaderType::Vertex,
            messages);
    }

} // namespace glslang
