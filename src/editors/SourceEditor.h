#ifndef SOURCEEDITOR_H
#define SOURCEEDITOR_H

#include "IEditor.h"
#include "FindReplaceBar.h"
#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;
class QSyntaxHighlighter;
class QCompleter;

class SourceEditor : public QPlainTextEdit, public IEditor
{
    Q_OBJECT
public:
    static bool load(const QString &fileName, QString *source);

    explicit SourceEditor(QString fileName, QWidget *parent = 0);
    ~SourceEditor();

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    QString source() const { return toPlainText(); }

    void setHighlighter(QSyntaxHighlighter *highlighter);
    void setCompleter(QCompleter *completer);
    QCompleter *completer() const { return mCompleter; }
    void findReplace();
    void setLineWrap(bool wrap) { setLineWrapMode(wrap ? WidgetWidth : NoWrap); }
    void setFont(const QFont &);
    void setTabSize(int tabSize);
    void setIndentWithSpaces(bool enabled);
    void setAutoIndentation(bool enabled);
    bool setCursorPosition(int line, int column);

signals:
    void fileNameChanged(const QString &fileName);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void updateViewportMargins();
    void updateExtraSelections();
    void updateLineNumberArea(const QRect&, int);
    void updateCompleterPopup(const QString &prefix, bool show);
    void insertCompletion(const QString &completion);
    void findReplaceAction(FindReplaceBar::Action action, QString find,
        QString replace, QTextDocument::FindFlags flags);

private:
    class LineNumberArea;

    QString tab() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void indentSelection(bool reverse = false);
    void autoIndentNewLine();
    void autoDeindentBrace();
    QString textUnderCursor() const;
    void markOccurrences(QString text, QTextDocument::FindFlags =
        QTextDocument::FindCaseSensitively | QTextDocument::FindWholeWords);

    QString mFileName;
    QScopedPointer<QTextDocument> mDocument;
    QSyntaxHighlighter *mHighlighter{ };
    QCompleter *mCompleter{ };
    LineNumberArea *mLineNumberArea{ };
    QTextCharFormat mCurrentLineFormat;
    QTextCharFormat mOccurrencesFormat;
    QList<QTextCursor> mMarkedOccurrences;
    QColor mLineNumberColor;
    int mTabSize{ };
    bool mIndentWithSpaces{ };
    bool mAutoIndentation{ };
};

#endif // SOURCEEDITOR_H
