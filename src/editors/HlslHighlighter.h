#ifndef HLSLHIGHLIGHTER_H
#define HLSLHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class QCompleter;

class HlslHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit HlslHighlighter(bool darkTheme, QObject *parent = nullptr);
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

#endif // HLSLHIGHLIGHTER_H
