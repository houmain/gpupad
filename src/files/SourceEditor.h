#ifndef SOURCEEDITOR_H
#define SOURCEEDITOR_H

#include "FileTypes.h"
#include "FindReplaceBar.h"
#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;
class QSyntaxHighlighter;
class QCompleter;
class ShaderCompiler;

class SourceEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    SourceEditor(SourceFilePtr file,
        QSyntaxHighlighter *highlighter, QCompleter *completer,
        QWidget *parent = 0);
    ~SourceEditor();

    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    const SourceFilePtr &file() const { return mFile; }

    void findReplace();
    void setLineWrap(bool wrap) { setLineWrapMode(wrap ? WidgetWidth : NoWrap); }
    void setFont(const QFont &);
    void setTabSize(int tabSize);
    void setIndentWithSpaces(bool enabled);
    void setAutoIndentation(bool enabled);
    bool setCursorPosition(int line, int column);
    void refreshShaderCompiler();

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

    SourceFilePtr mFile;
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

    QScopedPointer<ShaderCompiler> mShaderCompiler;
};

#endif // SOURCEEDITOR_H
