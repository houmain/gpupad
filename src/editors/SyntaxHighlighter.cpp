#include "SyntaxHighlighter.h"
#include "Syntax.h"
#include <QCompleter>
#include <QStringListModel>

namespace {
    const Syntax& getSyntax(SourceType sourceType) 
    {
        static const auto syntaxPlainText = new Syntax();
        static const auto syntaxGLSL = makeSyntaxGLSL();
        static const auto syntaxHLSL = makeSyntaxHLSL();
        static const auto syntaxJavaScript = makeSyntaxJavaScript();
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
        }
        return *syntaxPlainText;
    }
} // namespace

SyntaxHighlighter::SyntaxHighlighter(SourceType sourceType
    , bool darkTheme
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

    for (const auto &keyword : syntax.keywords()) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(keyword));
        rule.format = keywordFormat;
        mHighlightingRules.append(rule);
    }

    for (const auto &builtinFunction : syntax.builtinFunctions()) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(builtinFunction));
        rule.format = builtinFunctionFormat;
        mHighlightingRules.append(rule);
    }

    for (const auto &builtinConstant : syntax.builtinConstants()) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(builtinConstant));
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

    if (syntax.hasComments()) {
        mCommentRule.pattern = QRegularExpression("//.*");
        mCommentRule.format = commentFormat;    
        mCommentStartExpression = QRegularExpression("/\\*");
        mCommentEndExpression = QRegularExpression("\\*/");
    }

    mWhiteSpaceRule.pattern = QRegularExpression("\\s+", 
        QRegularExpression::UseUnicodePropertiesOption);
    mWhiteSpaceRule.format = whiteSpaceFormat;

    mCompleter = new QCompleter(this);
    auto completerModel = new QStringListModel(
        syntax.completerStrings(), mCompleter);
    mCompleter->setModel(completerModel);
    mCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mCompleter->setWrapAround(false);
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    const auto highlight = [&](const HighlightingRule &rule, bool override) {
        auto index = text.indexOf(rule.pattern);
        while (index >= 0) {
            const auto match = rule.pattern.match(text, index);
            const auto length = match.capturedLength();
            if (override || format(index) == QTextCharFormat{ })
                setFormat(index, length, rule.format);
            index = text.indexOf(rule.pattern, index + length);
        }
    };

    for (const HighlightingRule &rule : qAsConst(mHighlightingRules))
        highlight(rule, true);

    if (!mFunctionsRule.format.isEmpty())
        highlight(mFunctionsRule, false);

    if (!mCommentRule.format.isEmpty())
        highlight(mCommentRule, false);
    
    setCurrentBlockState(0);

    if (!mCommentRule.format.isEmpty()) {
        auto startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(mCommentStartExpression);

        while (startIndex >= 0) {
            // do not start multiline comment in single line comment or string
            if (startIndex && format(startIndex) != QTextCharFormat{ }) {
                setCurrentBlockState(0);
                break;
            }

            const auto match = mCommentEndExpression.match(text, startIndex);
            const auto endIndex = match.capturedStart();
            auto commentLength = 0;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            }
            else {
                commentLength = endIndex - startIndex + match.capturedLength();
            }
            setFormat(startIndex, commentLength, mCommentRule.format);
            startIndex = text.indexOf(mCommentStartExpression, startIndex + commentLength);
        }
    }

    highlight(mWhiteSpaceRule, true);
}