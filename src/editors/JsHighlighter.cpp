#include "JsHighlighter.h"
#include <QCompleter>
#include <QStringListModel>

namespace { const auto keywords = { "abstract", "arguments", "await",
    "boolean", "break", "byte", "case", "catch", "char", "class", "const",
    "continue", "debugger", "default", "delete", "do", "double", "else", "enum",
    "eval", "export", "extends", "false", "final", "finally", "float", "for",
    "function", "goto", "if", "implements", "import", "in", "instanceof", "int",
    "interface", "let", "long", "native", "new", "null", "package", "private",
    "protected", "public", "return", "short", "static", "super", "switch",
    "synchronized", "this", "throw", "throws", "transient", "true", "try",
    "typeof", "var", "void", "volatile", "while", "with", "yield",
    "function", "undefined", };

const auto builtinFunctions = { "Array", "Date", "eval", "hasOwnProperty",
    "Infinity", "isFinite", "isNaN", "isPrototypeOf", "length", "Math", "NaN",
    "name", "Number", "Object", "prototype", "String", "toString", "valueOf",
    "Math.abs", "Math.acos", "Math.acosh", "Math.asin", "Math.asinh", "Math.atan",
    "Math.atan2", "Math.atanh", "Math.cbrt", "Math.ceil", "Math.clz32", "Math.cos",
    "Math.cosh", "Math.exp", "Math.expm1", "Math.floor", "Math.fround", "Math.hypot",
    "Math.imul", "Math.log", "Math.log10", "Math.log1p", "Math.log2", "Math.max",
    "Math.min", "Math.pow", "Math.random", "Math.round", "Math.sign", "Math.sin",
    "Math.sinh", "Math.sqrt", "Math.tan", "Math.tanh", "Math.trunc"
};

const auto builtinConstants = {
    "Math.E", "Math.LN10", "Math.LN2", "Math.LOG10E", "Math.LOG2E",
    "Math.PI", "Math.SQRT1_2", "Math.SQRT2"
};

} // namespace

JsHighlighter::JsHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    QTextCharFormat builtinFunctionFormat;
    QTextCharFormat builtinConstantsFormat;
    QTextCharFormat preprocessorFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;

    functionFormat.setForeground(QColor("#000066"));
    //functionFormat.setFontWeight(QFont::Bold);
    keywordFormat.setForeground(QColor("#003C98"));
    builtinFunctionFormat.setForeground(QColor("#000066"));
    builtinConstantsFormat.setForeground(QColor("#981111"));
    numberFormat.setForeground(QColor("#981111"));
    quotationFormat.setForeground(QColor("#981111"));
    preprocessorFormat.setForeground(QColor("#800080"));
    singleLineCommentFormat.setForeground(QColor("#008700"));
    mMultiLineCommentFormat.setForeground(QColor("#008700"));

    auto rule = HighlightingRule();
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    mHighlightingRules.append(rule);

    auto completerStrings = QStringList();

    for (const auto& keyword : keywords) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(keyword));
        rule.format = keywordFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(keyword);
    }

    for (const auto& builtinFunction : builtinFunctions) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(builtinFunction));
        rule.format = builtinFunctionFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(builtinFunction);
    }

    for (const auto& builtinConstant : builtinConstants) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(builtinConstant));
        rule.format = builtinConstantsFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(builtinConstant);
    }

    rule.pattern = QRegExp("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?[uUlLfF]{,2}\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("\\b0x[0-9,A-F,a-f]+\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("\".*\"");
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("//[^\n]*");
    rule.format = singleLineCommentFormat;
    mHighlightingRules.append(rule);

    mCommentStartExpression = QRegExp("/\\*");
    mCommentEndExpression = QRegExp("\\*/");

    mCompleter = new QCompleter(this);
    completerStrings.sort(Qt::CaseInsensitive);
    auto completerModel = new QStringListModel(completerStrings, mCompleter);
    mCompleter->setModel(completerModel);
    mCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    mCompleter->setWrapAround(false);
}

void JsHighlighter::highlightBlock(const QString &text)
{
    foreach (const HighlightingRule &rule, mHighlightingRules) {
        auto index = rule.pattern.indexIn(text);
        while (index >= 0) {
            const auto length = rule.pattern.matchedLength();
            setFormat(index, length, rule.format);
            index = rule.pattern.indexIn(text, index + length);
        }
    }
    setCurrentBlockState(0);

    auto startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = mCommentStartExpression.indexIn(text);

    while (startIndex >= 0) {
        const auto endIndex = mCommentEndExpression.indexIn(text, startIndex);
        auto commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else {
            commentLength = endIndex - startIndex + mCommentEndExpression.matchedLength();
        }
        setFormat(startIndex, commentLength, mMultiLineCommentFormat);
        startIndex = mCommentStartExpression.indexIn(text, startIndex + commentLength);
    }
}
