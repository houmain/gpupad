#ifndef JSHIGHLIGHTER_H
#define JSHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>

class QCompleter;

class JsHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit JsHighlighter(bool darkTheme, QObject *parent = nullptr);
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
    QTextCharFormat mMultiLineCommentFormat;
};

#endif // JSHIGHLIGHTER_H
