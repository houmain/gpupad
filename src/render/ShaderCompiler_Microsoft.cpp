
#include "ShaderCompiler_Microsoft.h"
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <QRegularExpression>
#include <mutex>

namespace ShaderCompiler {

    namespace {
        QString getTarget(Shader::ShaderType type)
        {
            switch (type) {
            case Shader::ShaderType::Vertex:         return "vs";
            case Shader::ShaderType::Fragment:       return "ps";
            case Shader::ShaderType::Geometry:       return "gs";
            case Shader::ShaderType::TessControl:    return "hs";
            case Shader::ShaderType::TessEvaluation: return "ds";
            case Shader::ShaderType::Compute:        return "cs";
            default:                                 return "";
            }
        }

        QString getTarget(Shader::ShaderType type, int majorVersion,
            int minorVersion)
        {
            const auto typeString = getTarget(type);
            if (typeString.isEmpty())
                return "";
            return QStringLiteral("%1_%2_%3")
                .arg(typeString)
                .arg(majorVersion)
                .arg(minorVersion);
        }

        void parseLog_D3DCompiler(const QString &log, MessagePtrSet &messages,
            ItemId itemId)
        {
            // FILENAME(100,30-32): SEVERITY X100: TEXT
            static const auto split = QRegularExpression(
                "((.*)" // 2. filename
                "\\((\\d+)," // 3. line
                "([^\\)]*)\\):\\s+)?" // 4. column
                "([^ ]+)\\s+" // 5. severity
                "([^:]+):\\s+" // 6. code
                "(.*)"); // 7. text

            const auto lines = log.split('\n', Qt::SkipEmptyParts);
            for (const auto &line : lines) {
                const auto match = split.match(line);
                const auto fileName = match.captured(2);
                const auto lineNumber = match.captured(3).toInt();
                const auto severity = match.captured(5);
                const auto text = match.captured(7);
                if (text.isEmpty())
                    continue;

                auto messageType = MessageType::ShaderWarning;
                if (severity.contains("Info", Qt::CaseInsensitive))
                    messageType = MessageType::ShaderInfo;
                if (severity.contains("Error", Qt::CaseInsensitive))
                    messageType = MessageType::ShaderError;

                messages += MessageList::insert(fileName, lineNumber,
                    messageType, text);
            }
        }

        void parseLog_DXC(const QString &log, MessagePtrSet &messages,
            const QStringList &fileNames, ItemId itemId)
        {
            static const auto splitLines = QRegularExpression("\n(?!\\s)",
                QRegularExpression::MultilineOption);

            // FILENAME:8:42: SEVERITY: TEXT
            static const auto splitLine =
                QRegularExpression("(([^:]+):" // 2. filename
                                   "(\\d+):" // 3. line
                                   "(\\d+):)?\\s*" // 4. column
                                   "([^:]+):\\s*" // 5. severity
                                   "(.*)",
                    QRegularExpression::DotMatchesEverythingOption); // 6. text

            const auto lines = log.split(splitLines);
            for (const auto &line : lines) {
                const auto match = splitLine.match(line);
                auto fileName = match.captured(2);
                const auto lineNumber = match.captured(3).toInt();
                const auto severity = match.captured(5);
                const auto text = match.captured(6);
                if (text.isEmpty())
                    continue;

                auto messageType = MessageType::ShaderWarning;
                if (severity.contains("Info", Qt::CaseInsensitive))
                    messageType = MessageType::ShaderInfo;
                if (severity.contains("Error", Qt::CaseInsensitive))
                    messageType = MessageType::ShaderError;

                if (fileName == "hlsl.hlsl")
                    fileName = fileNames[0];

                messages += MessageList::insert(fileName, lineNumber,
                    messageType, text);
            }
        }

        class DXCCompiler
        {
        private:
            ComPtr<IDxcUtils> mUtils;
            ComPtr<IDxcIncludeHandler> mIncludeHandler;
            ComPtr<IDxcCompiler3> mCompiler;

        public:
            class ArgumentList
            {
            private:
                std::vector<std::unique_ptr<wchar_t[]>> mArgumentBuffer;
                std::vector<const wchar_t *> mArguments;

            public:
                void add(const wchar_t *literal)
                {
                    mArguments.push_back(literal);
                }

                void add(const QString &string)
                {
                    auto buffer = mArgumentBuffer
                                      .emplace_back(std::make_unique<wchar_t[]>(
                                          string.length() + 1))
                                      .get();
                    string.toWCharArray(buffer);
                    buffer[string.length()] = L'\0';
                    mArguments.push_back(buffer);
                };

                size_t size() const { return mArguments.size(); }
                const wchar_t **data() { return mArguments.data(); }
            };

            DXCCompiler()
            {
                AssertIfFailed(
                    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils)));
                AssertIfFailed(
                    mUtils->CreateDefaultIncludeHandler(&mIncludeHandler));
                AssertIfFailed(DxcCreateInstance(CLSID_DxcCompiler,
                    IID_PPV_ARGS(&mCompiler)));
            }

            bool compile(ArgumentList &&arguments, const Input &input,
                MessagePtrSet &messages, ComPtr<ID3DBlob> &binary,
                ComPtr<ID3D12ShaderReflection> *d3dReflection = nullptr)
            {
                if (!mCompiler)
                    return false;

                Q_ASSERT(input.sources.size() == 1);
                auto sourceBuffer = DxcBuffer{};
                const auto sourceString = input.sources.front().toStdString();
                sourceBuffer.Ptr = sourceString.c_str();
                sourceBuffer.Size = sourceString.size();
                sourceBuffer.Encoding = DXC_CP_ACP;

                auto compileResult = ComPtr<IDxcResult>();
                if (FAILED(mCompiler->Compile(&sourceBuffer, arguments.data(),
                        static_cast<uint32_t>(arguments.size()),
                        mIncludeHandler.Get(), IID_PPV_ARGS(&compileResult))))
                    return false;

                auto errors = ComPtr<IDxcBlobUtf8>();
                compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors),
                    nullptr);
                if (errors && errors->GetStringLength() > 0) {
                    parseLog_DXC(
                        static_cast<const char *>(errors->GetBufferPointer()),
                        messages, input.fileNames, input.itemId);
                    return false;
                }

                AssertIfFailed(compileResult->GetOutput(DXC_OUT_OBJECT,
                    IID_PPV_ARGS(binary.GetAddressOf()), nullptr));
                Q_ASSERT(binary->GetBufferSize() > 0);

                if (d3dReflection) {
                    auto reflectionData = ComPtr<ID3DBlob>();
                    AssertIfFailed(compileResult->GetOutput(DXC_OUT_REFLECTION,
                        IID_PPV_ARGS(reflectionData.GetAddressOf()), nullptr));
                    auto reflectionBuffer = DxcBuffer{};
                    reflectionBuffer.Ptr = reflectionData->GetBufferPointer();
                    reflectionBuffer.Size = reflectionData->GetBufferSize();
                    reflectionBuffer.Encoding = DXC_CP_ACP;
                    AssertIfFailed(mUtils->CreateReflection(&reflectionBuffer,
                        IID_PPV_ARGS(d3dReflection->GetAddressOf())));
                }
                return true;
            }
        };

        bool compile_DXC(const Session &session, const Input &input,
            DXCCompiler::ArgumentList &&arguments, MessagePtrSet &messages,
            ComPtr<ID3DBlob> &binary,
            ComPtr<ID3D12ShaderReflection> *d3dReflection = nullptr)
        {
            const auto target = getTarget(input.shaderType, 6, 0);
            if (target.isEmpty()) {
                messages += MessageList::insert(input.itemId,
                    MessageType::UnsupportedShaderType);
                return false;
            }

            arguments.add(L"-E");
            arguments.add(input.entryPoint);

            arguments.add(L"-T");
            arguments.add(target);

            //arguments.add(DXC_ARG_WARNINGS_ARE_ERRORS);
            arguments.add(DXC_ARG_DEBUG);

            static std::mutex sMutex;
            static DXCCompiler sCompiler;
            const auto guard = std::lock_guard(sMutex);
            return sCompiler.compile(std::move(arguments), input, messages,
                binary, d3dReflection);
        }
    } // namespace

    bool compileDXIL_D3DCompiler(const Session &session, const Input &input,
        MessagePtrSet &messages, ComPtr<ID3DBlob> &binary,
        ComPtr<ID3D12ShaderReflection> &d3dReflection)
    {
        const auto target = getTarget(input.shaderType, 5, 1);
        if (target.isEmpty()) {
            messages += MessageList::insert(input.itemId,
                MessageType::UnsupportedShaderType);
            return {};
        }

        auto flags1 = UINT{ D3DCOMPILE_DEBUG };
        auto flags2 = UINT{};
        auto error = ComPtr<ID3DBlob>();
        Q_ASSERT(input.sources.size() == 1);
        Q_ASSERT(input.fileNames.size() == 1);
        const auto sourceStr =
            std::string(qUtf8Printable(input.sources.first()));
        if (FAILED(D3DCompile(sourceStr.c_str(), sourceStr.size(),
                qUtf8Printable(input.fileNames.first()), nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                qUtf8Printable(input.entryPoint), qUtf8Printable(target),
                flags1, flags2, &binary, &error))) {
            const auto message = QString::fromUtf8(
                static_cast<const char *>(error->GetBufferPointer()));
            parseLog_D3DCompiler(message, messages, input.itemId);
            return false;
        }

        if (FAILED(D3DReflect(binary->GetBufferPointer(),
                binary->GetBufferSize(), IID_PPV_ARGS(&d3dReflection)))) {
            messages += MessageList::insert(input.itemId,
                MessageType::UnsupportedShaderType);
            return false;
        }
        return true;
    }

    bool compileDXIL(const Session &session, const Input &input,
        MessagePtrSet &messages, ComPtr<ID3DBlob> &binary,
        ComPtr<ID3D12ShaderReflection> &d3dReflection)
    {
        if (session.shaderCompiler == Session::ShaderCompiler::DXC)
            return compile_DXC(session, input, {}, messages, binary,
                &d3dReflection);

        if (session.shaderCompiler == Session::ShaderCompiler::D3DCompiler)
            return compileDXIL_D3DCompiler(session, input, messages, binary,
                d3dReflection);

        Q_ASSERT(!"unexpected shader compiler");
        return false;
    }

    Spirv compileSpirv_DXC(const Session &session, const Input &input,
        MessagePtrSet &messages)
    {
        auto arguments = DXCCompiler::ArgumentList();
        arguments.add(L"-spirv");

        //arguments.add(L"-ignore-line-directives");

        auto binary = ComPtr<ID3DBlob>();
        if (!compile_DXC(session, input, std::move(arguments), messages,
                binary))
            return {};

        const auto begin =
            reinterpret_cast<const uint32_t *>(binary->GetBufferPointer());
        const auto end = begin + binary->GetBufferSize() / sizeof(uint32_t);
        return Spirv(begin, end);
    }
} // namespace ShaderCompiler
