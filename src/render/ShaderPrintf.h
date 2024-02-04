#pragma once

#include "MessageList.h"
#include "session/Item.h"

class ShaderPrintf
{
public:
    static QString requiredVersion() { return "#version 430"; }
    static QString preamble();

    const char *bufferBindingName() const { return "_printfBuffer"; }
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

    static ParsedFormatString parseFormatString(QStringView string);
    static QString formatMessage(const ParsedFormatString &format,
        const QList<Argument>& arguments);

    QSet<Shader::ShaderType> mUsedInStages;
    QList<ParsedFormatString> mFormatStrings;
};
