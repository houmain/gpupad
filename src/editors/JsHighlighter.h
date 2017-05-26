#ifndef JSHIGHLIGHTER_H
#define JSHIGHLIGHTER_H

#include <QSyntaxHighlighter>

class QCompleter;

class JsHighlighter : public QSyntaxHighlighter {
public:
    explicit JsHighlighter(QObject *parent = 0);
    void highlightBlock(const QString &text) override;
    QCompleter *completer() const { return mCompleter; }

private:
    struct HighlightingRule {
        QRegExp pattern;
        QTextCharFormat format;
    };
    QCompleter *mCompleter{ };
    QVector<HighlightingRule> mHighlightingRules;
    QRegExp mCommentStartExpression;
    QRegExp mCommentEndExpression;
    QTextCharFormat mMultiLineCommentFormat;
};

#endif // JSHIGHLIGHTER_H
