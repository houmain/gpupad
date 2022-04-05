#include "SyntaxHighlighter.h"
#include "Syntax.h"

namespace {
    const Syntax& getSyntax(SourceType sourceType) 
    {
        static const auto syntaxPlainText = makeSyntaxPlain();
        static const auto syntaxGLSL = makeSyntaxGLSL();
        static const auto syntaxHLSL = makeSyntaxHLSL();
        static const auto syntaxJavaScript = makeSyntaxJavaScript();
        static const auto syntaxLua = makeSyntaxLua();
        switch (sourceType) {
            case SourceType::PlainText:
                  return *syntaxPlainText;
            case SourceType::GLSL_VertexShader:
            case SourceType::GLSL_FragmentShader:
            case SourceType::GLSL_GeometryShader:
            case SourceType::GLSL_TessellationControl:
            case SourceType::GLSL_TessellationEvaluation:
            case SourceType::GLSL_ComputeShader:
                return *syntaxGLSL;
            case SourceType::HLSL_VertexShader:
            case SourceType::HLSL_PixelShader:
            case SourceType::HLSL_GeometryShader:
            case SourceType::HLSL_DomainShader:
            case SourceType::HLSL_HullShader:
            case SourceType::HLSL_ComputeShader:
                return *syntaxHLSL;
            case SourceType::JavaScript: 
                return *syntaxJavaScript;
            case SourceType::Lua:
                return *syntaxLua;
        }
        return *syntaxPlainText;
    }
} // namespace

SyntaxHighlighter::SyntaxHighlighter(SourceType sourceType
    , bool darkTheme
    , bool showWhiteSpace
    , QObject *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    QTextCharFormat builtinFunctionFormat;
    QTextCharFormat builtinConstantsFormat;
    QTextCharFormat preprocessorFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat whiteSpaceFormat;

    functionFormat.setFontWeight(QFont::Bold);

    if (darkTheme) {
        functionFormat.setForeground(QColor(0x7AAFFF));
        keywordFormat.setForeground(QColor(0x7AAFFF));
        builtinFunctionFormat.setForeground(QColor(0x7AAFFF));
        builtinConstantsFormat.setForeground(QColor(0xDD8D8D));
        numberFormat.setForeground(QColor(0xB09D30));
        quotationFormat.setForeground(QColor(0xB09D30));
        preprocessorFormat.setForeground(QColor(0xC87FFF));
        commentFormat.setForeground(QColor(0x56C056));
        whiteSpaceFormat.setForeground(QColor(0x666666));
    }
    else {
        functionFormat.setForeground(QColor(0x000066));        
        keywordFormat.setForeground(QColor(0x003C98));
        builtinFunctionFormat.setForeground(QColor(0x000066));
        builtinConstantsFormat.setForeground(QColor(0x981111));
        numberFormat.setForeground(QColor(0x981111));
        quotationFormat.setForeground(QColor(0x981111));
        preprocessorFormat.setForeground(QColor(0x800080));
        commentFormat.setForeground(QColor(0x008700));
        whiteSpaceFormat.setForeground(QColor(0xCCCCCC));
    }

    const auto& syntax = getSyntax(sourceType);
    auto rule = HighlightingRule();

    const auto keywords = syntax.keywords();
    for (const auto &keyword : keywords) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(keyword));
        rule.format = keywordFormat;
        mHighlightingRules.append(rule);
    }

    const auto builtinFunctions = syntax.builtinFunctions();
    for (const auto &builtinFunction : builtinFunctions) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(builtinFunction));
        rule.format = builtinFunctionFormat;
        mHighlightingRules.append(rule);
    }

    const auto builtinConstants = syntax.builtinConstants();
    for (const auto &builtinConstant : builtinConstants) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1(\\.[_A-Za-z0-9]+)*\\b").arg(builtinConstant));
        rule.format = builtinConstantsFormat;
        mHighlightingRules.append(rule);
    }

    rule.pattern = QRegularExpression("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[uU]*[lLfF]?\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression("\\b0x[0-9,A-F,a-f]+[uU]*[lL]?\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    auto quotation = QString("%1([^%1]*(\\\\%1[^%1]*)*)(%1|$)");
    rule.pattern = QRegularExpression(quotation.arg('"'));
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression(quotation.arg('\''));
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegularExpression(quotation.arg('`'));
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    if (syntax.hasPreprocessor()) {
        rule.pattern = QRegularExpression("^\\s*#.*");
        rule.format = preprocessorFormat;
        mHighlightingRules.append(rule);
    }

    if (syntax.hasFunctions()) {
        mFunctionsRule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()");
        mFunctionsRule.format = functionFormat;
    }

    if (!syntax.singleLineCommentBegin().isEmpty()) {
        mSingleLineCommentRule.pattern = QRegularExpression(syntax.singleLineCommentBegin());
        mSingleLineCommentRule.format = commentFormat;
    }
    if (!syntax.multiLineCommentBegin().isEmpty()) {
        mMultiLineCommentStart = QRegularExpression(syntax.multiLineCommentBegin());
        mMultiLineCommentEnd = QRegularExpression(syntax.multiLineCommentEnd());
        mMultiLineCommentFormat = commentFormat;
    }

    if (showWhiteSpace) {
        mWhiteSpaceRule.pattern = QRegularExpression("\\s+", 
            QRegularExpression::UseUnicodePropertiesOption);
        mWhiteSpaceRule.format = whiteSpaceFormat;
    }
    mCompleterStrings = syntax.completerStrings();
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    const auto highlight = [&](const HighlightingRule &rule, bool override) {
        for (auto index = text.indexOf(rule.pattern); index >= 0; ) {
            const auto match = rule.pattern.match(text, index);
            const auto length = match.capturedLength();
            if (override || format(index) == QTextCharFormat{ })
                setFormat(index, length, rule.format);
            index = text.indexOf(rule.pattern, index + length);
        }
    };

    if (!mFunctionsRule.format.isEmpty())
        highlight(mFunctionsRule, false);

    for (const HighlightingRule &rule : qAsConst(mHighlightingRules))
        highlight(rule, true);

    if (!mSingleLineCommentRule.format.isEmpty())
        highlight(mSingleLineCommentRule, false);
    
    if (!mMultiLineCommentFormat.isEmpty()) {
        setCurrentBlockState(0);

        auto startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(mMultiLineCommentStart);

        while (startIndex >= 0) {
            // do not start multiline comment in single line comment or string
            if (startIndex && format(startIndex) != QTextCharFormat{ }) {
                setCurrentBlockState(0);
                break;
            }

            const auto match = mMultiLineCommentEnd.match(text, startIndex);
            const auto endIndex = match.capturedStart();
            auto commentLength = 0;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            }
            else {
                commentLength = endIndex - startIndex + match.capturedLength();
            }
            setFormat(startIndex, commentLength, mMultiLineCommentFormat);
            startIndex = text.indexOf(mMultiLineCommentStart, startIndex + commentLength);
        }
    }

    if (!mWhiteSpaceRule.format.isEmpty())
        highlight(mWhiteSpaceRule, true);
}
