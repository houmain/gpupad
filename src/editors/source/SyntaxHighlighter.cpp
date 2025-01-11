#include "SyntaxHighlighter.h"
#include "Syntax.h"
#include "Theme.h"
#include <QMap>

struct SyntaxHighlighter::Data
{
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightingRule> highlightingRules;
    HighlightingRule functionsRule;
    HighlightingRule singleLineCommentRule;
    QTextCharFormat multiLineCommentFormat;
    QRegularExpression multiLineCommentStart;
    QRegularExpression multiLineCommentEnd;
    HighlightingRule whiteSpaceRule;
    QStringList completerStrings;
};

namespace {
    const Syntax &getSyntax(SourceType sourceType)
    {
        static const auto SyntaxGeneric = makeSyntaxGeneric();
        static const auto syntaxGLSL = makeSyntaxGLSL();
        static const auto syntaxHLSL = makeSyntaxHLSL();
        static const auto syntaxJavaScript = makeSyntaxJavaScript();
        switch (sourceType) {
        case SourceType::PlainText:
        case SourceType::Generic:                    break;
        case SourceType::GLSL_VertexShader:
        case SourceType::GLSL_FragmentShader:
        case SourceType::GLSL_GeometryShader:
        case SourceType::GLSL_TessControlShader:
        case SourceType::GLSL_TessEvaluationShader:
        case SourceType::GLSL_ComputeShader:
        case SourceType::GLSL_TaskShader:
        case SourceType::GLSL_MeshShader:
        case SourceType::GLSL_RayGenerationShader:
        case SourceType::GLSL_RayIntersectionShader:
        case SourceType::GLSL_RayAnyHitShader:
        case SourceType::GLSL_RayClosestHitShader:
        case SourceType::GLSL_RayMissShader:
        case SourceType::GLSL_RayCallableShader:     return *syntaxGLSL;
        case SourceType::HLSL_VertexShader:
        case SourceType::HLSL_PixelShader:
        case SourceType::HLSL_GeometryShader:
        case SourceType::HLSL_DomainShader:
        case SourceType::HLSL_HullShader:
        case SourceType::HLSL_ComputeShader:
        case SourceType::HLSL_AmplificationShader:
        case SourceType::HLSL_MeshShader:
        case SourceType::HLSL_RayGenerationShader:
        case SourceType::HLSL_RayIntersectionShader:
        case SourceType::HLSL_RayAnyHitShader:
        case SourceType::HLSL_RayClosestHitShader:
        case SourceType::HLSL_RayMissShader:
        case SourceType::HLSL_RayCallableShader:     return *syntaxHLSL;
        case SourceType::JavaScript:                 return *syntaxJavaScript;
        }
        return *SyntaxGeneric;
    }

    QSharedPointer<const SyntaxHighlighter::Data> createData(
        const Syntax &syntax, const Theme &theme, bool showWhiteSpace)
    {
        auto keywordFormat = QTextCharFormat();
        auto builtinFunctionFormat = QTextCharFormat();
        auto builtinConstantsFormat = QTextCharFormat();
        auto preprocessorFormat = QTextCharFormat();
        auto quotationFormat = QTextCharFormat();
        auto numberFormat = QTextCharFormat();
        auto functionFormat = QTextCharFormat();
        auto commentFormat = QTextCharFormat();
        auto whiteSpaceFormat = QTextCharFormat();

        functionFormat.setFontWeight(QFont::Bold);
        functionFormat.setForeground(theme.getColor(ThemeColor::Function));
        keywordFormat.setForeground(theme.getColor(ThemeColor::Keyword));
        builtinFunctionFormat.setForeground(
            theme.getColor(ThemeColor::BuiltinFunction));
        builtinConstantsFormat.setForeground(
            theme.getColor(ThemeColor::BuiltinConstant));
        numberFormat.setForeground(theme.getColor(ThemeColor::Number));
        quotationFormat.setForeground(theme.getColor(ThemeColor::Quotation));
        preprocessorFormat.setForeground(
            theme.getColor(ThemeColor::Preprocessor));
        commentFormat.setForeground(theme.getColor(ThemeColor::Comment));
        whiteSpaceFormat.setForeground(theme.getColor(ThemeColor::WhiteSpace));

        auto d = SyntaxHighlighter::Data{};
        auto rule = SyntaxHighlighter::Data::HighlightingRule{};

        const auto keywords = syntax.keywords();
        for (const auto &keyword : keywords) {
            rule.pattern =
                QRegularExpression(QStringLiteral("\\b%1\\b").arg(keyword));
            rule.format = keywordFormat;
            d.highlightingRules.append(rule);
        }

        const auto builtinFunctions = syntax.builtinFunctions();
        for (const auto &builtinFunction : builtinFunctions) {
            rule.pattern = QRegularExpression(
                QStringLiteral("\\b%1\\b").arg(builtinFunction));
            rule.format = builtinFunctionFormat;
            d.highlightingRules.append(rule);
        }

        const auto builtinConstants = syntax.builtinConstants();
        for (const auto &builtinConstant : builtinConstants) {
            rule.pattern =
                QRegularExpression(QStringLiteral("\\b%1(\\.[_A-Za-z0-9]+)*\\b")
                                       .arg(builtinConstant));
            rule.format = builtinConstantsFormat;
            d.highlightingRules.append(rule);
        }

        rule.pattern = QRegularExpression(
            "\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[uU]*[lLfF]?\\b");
        rule.format = numberFormat;
        d.highlightingRules.append(rule);

        rule.pattern = QRegularExpression("\\b0x[0-9,A-F,a-f]+[uU]*[lL]?\\b");
        rule.format = numberFormat;
        d.highlightingRules.append(rule);

        const auto quotation = QString("%1([^%1]*(\\\\%1[^%1]*)*)(%1|$)");
        rule.pattern = QRegularExpression(quotation.arg('"'));
        rule.format = quotationFormat;
        d.highlightingRules.append(rule);

        rule.pattern = QRegularExpression(quotation.arg('\''));
        rule.format = quotationFormat;
        d.highlightingRules.append(rule);

        rule.pattern = QRegularExpression(quotation.arg('`'));
        rule.format = quotationFormat;
        d.highlightingRules.append(rule);

        if (syntax.hasPreprocessor()) {
            rule.pattern = QRegularExpression("^\\s*#.*");
            rule.format = preprocessorFormat;
            d.highlightingRules.append(rule);
        }

        if (syntax.hasFunctions()) {
            d.functionsRule.pattern =
                QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()");
            d.functionsRule.format = functionFormat;
        }

        if (!syntax.singleLineCommentBegin().isEmpty()) {
            d.singleLineCommentRule.pattern =
                QRegularExpression(syntax.singleLineCommentBegin());
            d.singleLineCommentRule.format = commentFormat;
        }
        if (!syntax.multiLineCommentBegin().isEmpty()) {
            d.multiLineCommentStart =
                QRegularExpression(syntax.multiLineCommentBegin());
            d.multiLineCommentEnd =
                QRegularExpression(syntax.multiLineCommentEnd());
            d.multiLineCommentFormat = commentFormat;
        }

        if (showWhiteSpace) {
            d.whiteSpaceRule.pattern = QRegularExpression("\\s+",
                QRegularExpression::UseUnicodePropertiesOption);
            d.whiteSpaceRule.format = whiteSpaceFormat;
        }
        d.completerStrings = syntax.completerStrings();

        return QSharedPointer<const SyntaxHighlighter::Data>(
            new SyntaxHighlighter::Data(std::move(d)));
    }

    QSharedPointer<const SyntaxHighlighter::Data> getData(SourceType sourceType,
        const Theme &theme, bool showWhiteSpace)
    {
        const auto &syntax = getSyntax(sourceType);
        const auto key =
            std::make_tuple(&syntax, theme.fileName(), showWhiteSpace);
        static auto cache = std::map<decltype(key),
            QSharedPointer<const SyntaxHighlighter::Data>>();
        if (!cache[key])
            cache[key] = createData(syntax, theme, showWhiteSpace);
        return cache[key];
    }
} // namespace

SyntaxHighlighter::SyntaxHighlighter(SourceType sourceType, const Theme &theme,
    bool showWhiteSpace, QObject *parent)
    : QSyntaxHighlighter(parent)
    , mData(getData(sourceType, theme, showWhiteSpace))
{
}

const QStringList &SyntaxHighlighter::completerStrings() const
{
    return mData->completerStrings;
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    const auto highlight = [&](const auto &rule, bool override) {
        for (auto index = text.indexOf(rule.pattern); index >= 0;) {
            const auto match = rule.pattern.match(text, index);
            const auto length = match.capturedLength();
            if (override || format(index) == QTextCharFormat{})
                setFormat(index, length, rule.format);
            index = text.indexOf(rule.pattern, index + length);
        }
    };

    const auto &d = *mData;

    if (!d.functionsRule.format.isEmpty())
        highlight(d.functionsRule, false);

    for (const auto &rule : std::as_const(d.highlightingRules))
        highlight(rule, true);

    if (!d.singleLineCommentRule.format.isEmpty())
        highlight(d.singleLineCommentRule, false);

    if (!d.multiLineCommentFormat.isEmpty()) {
        setCurrentBlockState(0);

        auto startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(d.multiLineCommentStart);

        while (startIndex >= 0) {
            // do not start multiline comment in single line comment or string
            if (startIndex && format(startIndex) != QTextCharFormat{}) {
                setCurrentBlockState(0);
                break;
            }

            const auto match = d.multiLineCommentEnd.match(text, startIndex);
            const auto endIndex = match.capturedStart();
            auto commentLength = 0;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + match.capturedLength();
            }
            setFormat(startIndex, commentLength, d.multiLineCommentFormat);
            startIndex = text.indexOf(d.multiLineCommentStart,
                startIndex + commentLength);
        }
    }

    if (!d.whiteSpaceRule.format.isEmpty())
        highlight(d.whiteSpaceRule, true);
}
