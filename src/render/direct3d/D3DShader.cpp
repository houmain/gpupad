#include "D3DShader.h"
#include <optional>
#include <d3dcompiler.h>
#include <dxcapi.h>

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

    void parseD3DCompileLog(const QString &log, MessagePtrSet &messages,
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

            messages +=
                MessageList::insert(fileName, lineNumber, messageType, text);
        }
    }

    void parseDXCLog(const QString &log, MessagePtrSet &messages,
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

            messages +=
                MessageList::insert(fileName, lineNumber, messageType, text);
        }
    }
} // namespace

D3DShader::D3DShader(Shader::ShaderType type,
    const QList<const Shader *> &shaders, const Session &session)
    : ShaderBase(type, shaders, session)
{
}

QStringList D3DShader::preprocessorDefinitions() const
{
    auto definitions = ShaderBase::preprocessorDefinitions();
    definitions.append("GPUPAD_DIRECT3D 1");
    return definitions;
}

const SpvReflectDescriptorBinding *D3DShader::getSpirvDescriptorBinding(
    const QString &name) const
{
    if (!mReflection)
        return nullptr;

    if (isGlobalUniformBlockName(name))
        for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
            const auto &binding = mReflection->descriptor_bindings[i];
            if (isGlobalUniformBlockName(binding.type_description->type_name))
                return &binding;
        }

    for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
        const auto &binding = mReflection->descriptor_bindings[i];
        if (binding.type_description->type_name == name)
            return &binding;
    }

    for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
        const auto &binding = mReflection->descriptor_bindings[i];
        if (binding.name == name)
            return &binding;
    }

    if (name.startsWith("_")) {
        auto ok = false;
        const auto spirvId = name.mid(1).toUInt(&ok);
        if (ok) {
            for (auto i = 0u; i < mReflection->descriptor_binding_count; ++i) {
                const auto &binding = mReflection->descriptor_bindings[i];
                if (binding.spirv_id == spirvId)
                    return &binding;
            }
        }
    }
    return nullptr;
}

bool D3DShader::compile(PrintfBase &printf)
{
    if (mBinary)
        return true;

    if (mSession.shaderCompiler == Session::ShaderCompiler::glslang) {
        const auto spirv = compileSpirv(printf);
        if (!spirv)
            return false;
        if (!compile(Spirv::generateHLSL(spirv, mItemId, mMessages)))
            return false;
        mReflection = Reflection(spirv.spirv());
        return true;
    }

    const auto patchedSources = getPatchedSourcesHLSL(printf);
    if (patchedSources.isEmpty())
        return false;

    return compile(patchedSources.join("\n"));
}

bool D3DShader::compile(const QString &source)
{
    if (source.isEmpty())
        return false;

    if (mSession.shaderCompiler == Session::ShaderCompiler::D3DCompiler)
        return compileD3DCompile(source);

    return compileDXC(source);
}

bool D3DShader::compileD3DCompile(const QString &source)
{
    const auto target = getTarget(mType, 5, 1);
    if (target.isEmpty()) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::UnsupportedShaderType);
        return {};
    }

    auto flags1 = UINT{ D3DCOMPILE_DEBUG };
    auto flags2 = UINT{};
    auto error = ComPtr<ID3DBlob>();
    auto binary = ComPtr<ID3DBlob>();
    const auto sourceStr = std::string(qUtf8Printable(source));
    if (FAILED(D3DCompile(sourceStr.c_str(), sourceStr.size(),
            qUtf8Printable(mFileNames.first()), nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE, qUtf8Printable(mEntryPoint),
            qUtf8Printable(target), flags1, flags2, &binary, &error))) {
        const auto message = QString::fromUtf8(
            static_cast<const char *>(error->GetBufferPointer()));
        parseD3DCompileLog(message, mMessages, mItemId);
        return false;
    }

    if (FAILED(D3DReflect(binary->GetBufferPointer(), binary->GetBufferSize(),
            IID_PPV_ARGS(&mD3DReflection)))) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::UnsupportedShaderType);
        return false;
    }
    mBinary = std::move(binary);
    return true;
}

bool D3DShader::compileDXC(const QString &source)
{
    const auto target = getTarget(mType, 6, 0);
    if (target.isEmpty()) {
        mMessages +=
            MessageList::insert(mItemId, MessageType::UnsupportedShaderType);
        return {};
    }

    auto argumentBuffer = std::vector<std::unique_ptr<wchar_t[]>>();
    auto arguments = std::vector<const wchar_t *>();

    const auto pushStringArgument = [&](const QString &string) {
        auto buffer =
            argumentBuffer
                .emplace_back(std::make_unique<wchar_t[]>(string.length() + 1))
                .get();
        string.toWCharArray(buffer);
        buffer[string.length()] = L'\0';
        arguments.push_back(buffer);
    };

    arguments.push_back(L"-E");
    pushStringArgument(mEntryPoint);

    arguments.push_back(L"-T");
    pushStringArgument(target);

    arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
    arguments.push_back(DXC_ARG_DEBUG);

    auto sourceBuffer = DxcBuffer{};
    const auto sourceStr = std::string(qUtf8Printable(source));
    sourceBuffer.Ptr = sourceStr.c_str();
    sourceBuffer.Size = sourceStr.size();
    sourceBuffer.Encoding = 0;

    auto utils = ComPtr<IDxcUtils>();
    auto hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

    auto includeHandler = ComPtr<IDxcIncludeHandler>();
    utils->CreateDefaultIncludeHandler(&includeHandler);

    auto compiler = ComPtr<IDxcCompiler3>();
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    auto compileResult = ComPtr<IDxcResult>();
    compiler->Compile(&sourceBuffer, arguments.data(),
        static_cast<uint32_t>(arguments.size()), includeHandler.Get(),
        IID_PPV_ARGS(&compileResult));

    auto errors = ComPtr<IDxcBlobUtf8>();
    compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        auto message = static_cast<const char *>(errors->GetBufferPointer());
        parseDXCLog(message, mMessages, mFileNames, mItemId);
        return false;
    }

    auto binary = ComPtr<ID3DBlob>();
    AssertIfFailed(compileResult->GetOutput(DXC_OUT_OBJECT,
        IID_PPV_ARGS(binary.GetAddressOf()), nullptr));

    auto reflectionData = ComPtr<ID3DBlob>();
    hr = compileResult->GetOutput(DXC_OUT_REFLECTION,
        IID_PPV_ARGS(reflectionData.GetAddressOf()), nullptr);
    auto reflectionBuffer = DxcBuffer{};
    reflectionBuffer.Ptr = reflectionData->GetBufferPointer();
    reflectionBuffer.Size = reflectionData->GetBufferSize();
    reflectionBuffer.Encoding = 0;
    AssertIfFailed(utils->CreateReflection(&reflectionBuffer,
        IID_PPV_ARGS(mD3DReflection.GetAddressOf())));

    mBinary = std::move(binary);
    return true;
}
