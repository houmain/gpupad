#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include "SourceType.h"

class QCompleter;

class SyntaxHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    SyntaxHighlighter(SourceType sourceType, 
        bool darkTheme, bool showWhiteSpace, QObject *parent = nullptr);
    void highlightBlock(const QString &text) override;
    const QStringList &completerStrings() const { return mCompleterStrings; }

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> mHighlightingRules;
    HighlightingRule mFunctionsRule;
    HighlightingRule mSingleLineCommentRule;
    QTextCharFormat mMultiLineCommentFormat;
    QRegularExpression mMultiLineCommentStart;
    QRegularExpression mMultiLineCommentEnd;
    HighlightingRule mWhiteSpaceRule;
    QStringList mCompleterStrings;
};
