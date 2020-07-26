#include "JsHighlighter.h"
#include <QCompleter>
#include <QStringListModel>

namespace {

// https://developer.mozilla.org/de/docs/Web/JavaScript/Reference/Lexical_grammar
const auto keywords = {
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "export", "extends", "finally", "for", "function",
    "if", "import", "in", "instanceof", "new", "return", "super", "switch", "this",
    "throw", "try", "typeof", "var", "void", "while", "with", "yield", "enum",
    "implements", "interface", "let", "package", "private", "protected", "public",
    "static", "await", "of" };

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
const auto globalObjects = {
    "Infinity", "NaN", "undefined", "null", "eval", "isFinite", "isNaN",
    "parseFloat", "parseInt", "decodeURI", "decodeURIComponent", "encodeURI",
    "encodeURIComponent", "Object", "Boolean", "Error", "EvalError", // "Function",
    "RangeError", "ReferenceError", "SyntaxError", "TypeError", "URIError",
    "Number", "Math", "Date", "String", "RegExp", "Array", "Int8Array",
    "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array",
    "Uint32Array", "Float32Array", "Float64Array", "Map", "Set", "WeakMap",
    "WeakSet", "ArrayBuffer", "SharedArrayBuffer", "Atomics", "DataView", "JSON",

    "true", "false", "console", "print" };
} // namespace

JsHighlighter::JsHighlighter(bool darkTheme, QObject *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    QTextCharFormat globalObjectFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;

    if (darkTheme) {
        functionFormat.setForeground(QColor(0x7AAFFF));
        keywordFormat.setForeground(QColor(0x7AAFFF));
        globalObjectFormat.setForeground(QColor(0x7AAFFF));
        numberFormat.setForeground(QColor(0xB09D30));
        quotationFormat.setForeground(QColor(0xB09D30));
        singleLineCommentFormat.setForeground(QColor(0x56C056));
        mMultiLineCommentFormat.setForeground(QColor(0x56C056));
    }
    else {
        functionFormat.setForeground(QColor(0x000066));
        keywordFormat.setForeground(QColor(0x003C98));
        globalObjectFormat.setForeground(QColor(0x003C98));
        numberFormat.setForeground(QColor(0x981111));
        quotationFormat.setForeground(QColor(0x981111));
        singleLineCommentFormat.setForeground(QColor(0x008700));
        mMultiLineCommentFormat.setForeground(QColor(0x008700));
    }

    auto rule = HighlightingRule();
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    mHighlightingRules.append(rule);

    auto completerStrings = QStringList();

    for (const auto &keyword : keywords) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(keyword));
        rule.format = keywordFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(keyword);
    }

    for (const auto &global : globalObjects) {
        rule.pattern = QRegExp(QStringLiteral("\\b%1\\b").arg(global));
        rule.format = globalObjectFormat;
        mHighlightingRules.append(rule);
        completerStrings.append(global);
    }

    rule.pattern = QRegExp("\\b[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("\\b0x[0-9,A-F,a-f]+\\b");
    rule.format = numberFormat;
    mHighlightingRules.append(rule);

    auto quotation = QString("%1([^%1]*(\\\\%1[^%1]*)*)(%1|$)");
    rule.pattern = QRegExp(quotation.arg('"'));
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp(quotation.arg('\''));
    rule.format = quotationFormat;
    mHighlightingRules.append(rule);

    rule.pattern = QRegExp("//.*");
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
    for (const HighlightingRule &rule : qAsConst(mHighlightingRules)) {
        auto index = rule.pattern.indexIn(text);
        while (index >= 0) {
            const auto length = rule.pattern.matchedLength();
            // do not start single line comment in string
            if (format(index) == QTextCharFormat{ })
                setFormat(index, length, rule.format);
            index = rule.pattern.indexIn(text, index + length);
        }
    }
    setCurrentBlockState(0);

    auto startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = mCommentStartExpression.indexIn(text);

    while (startIndex >= 0) {
        // do not start multiline comment in single line comment or string
        if (startIndex && format(startIndex) != QTextCharFormat{ }) {
            setCurrentBlockState(0);
            break;
        }

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
