#ifndef GLSLHIGHLIGHTER_H
#define GLSLHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class QCompleter;

class GlslHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit GlslHighlighter(bool darkTheme, QObject *parent = nullptr);
    void highlightBlock(const QString &text) override;
    QCompleter *completer() const { return mCompleter; }

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QCompleter *mCompleter{ };
    QVector<HighlightingRule> mHighlightingRules;
    QRegularExpression mCommentStartExpression;
    QRegularExpression mCommentEndExpression;
    QTextCharFormat mCommentFormat;
};

#endif // GLSLHIGHLIGHTER_H
