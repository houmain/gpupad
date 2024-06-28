#pragma once

#include "MessageList.h"
#include "session/Item.h"
#include <span>

class ShaderPrintf
{
public:
    static QString requiredVersionGLSL() { return QStringLiteral("#version 430"); }
    static QString bufferBindingName() { return QStringLiteral("_printfBuffer"); }
    static QString preambleGLSL();
    static QString preambleHLSL();
    
    bool isUsed() const;
    bool isUsed(Shader::ShaderType stage) const;
    QString patchSource(Shader::ShaderType stage,
        const QString &fileName, const QString &source);

protected:
    static const auto maxBufferValues = 1024 - 2;

    struct ParsedFormatString {
        QStringList text;
        QStringList argumentFormats;
        QString fileName;
        int line;
    };
    struct Argument {
        uint32_t type;
        QList<uint32_t> values;
    };
    struct BufferHeader { 
        uint32_t offset;
        uint32_t prevBegin; 
    };

    static BufferHeader initializeHeader();
    static ParsedFormatString parseFormatString(QStringView string);
    static QString formatMessage(const ParsedFormatString &format,
        const QList<Argument>& arguments);
    MessagePtrSet formatMessages(ItemId callItemId,
        const BufferHeader &header, std::span<const uint32_t> data);

    QSet<Shader::ShaderType> mUsedInStages;
    QList<ParsedFormatString> mFormatStrings;
};
