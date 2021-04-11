#pragma once

#include "GLItem.h"

class GLPrintf
{
public:
    static QString requiredVersion() { return "#version 430"; }
    static QString preamble();

    const char *bufferBindingName() const { return "_printfBuffer"; }
    const GLObject &bufferObject() const { return mBufferObject; }
    bool isUsed() const;
    bool isUsed(Shader::ShaderType stage) const;
    QString patchSource(Shader::ShaderType stage,
        const QString &fileName, const QString &source);
    void clear();
    MessagePtrSet formatMessages(ItemId callItemId);

private:
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

    static ParsedFormatString parseFormatString(QStringView string);
    static QString formatMessage(const ParsedFormatString &format,
        const QList<Argument>& arguments);

    QSet<Shader::ShaderType> mUsedInStages;
    QList<ParsedFormatString> mFormatStrings;
    GLObject mBufferObject;
};
