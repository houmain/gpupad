#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include "SourceType.h"

class QCompleter;

class SyntaxHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    SyntaxHighlighter(SourceType sourceType, 
        bool darkTheme, QObject *parent = nullptr);
    void highlightBlock(const QString &text) override;
    QCompleter *completer() const { return mCompleter; }

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QCompleter *mCompleter{ };
    QVector<HighlightingRule> mHighlightingRules;
    HighlightingRule mFunctionsRule;
    HighlightingRule mCommentRule;
    QRegularExpression mCommentStartExpression;
    QRegularExpression mCommentEndExpression;
    HighlightingRule mWhiteSpaceRule;
};
