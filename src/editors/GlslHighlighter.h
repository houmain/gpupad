#ifndef GLSLHIGHLIGHTER_H
#define GLSLHIGHLIGHTER_H

#include <QSyntaxHighlighter>

class QCompleter;

class GlslHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit GlslHighlighter(bool darkTheme, QObject *parent = nullptr);
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

#endif // GLSLHIGHLIGHTER_H
