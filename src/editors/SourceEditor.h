#ifndef SOURCEEDITOR_H
#define SOURCEEDITOR_H

#include "IEditor.h"
#include "FindReplaceBar.h"
#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;
class QSyntaxHighlighter;
class QCompleter;

class SourceEditor final : public QPlainTextEdit, public IEditor
{
    Q_OBJECT
public:
    explicit SourceEditor(QString fileName,
        FindReplaceBar *findReplaceBar, QWidget *parent = nullptr);
    ~SourceEditor() override;

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool reload() override;
    bool save() override;
    int tabifyGroup() override { return 0; }
    QString source() const { return toPlainText(); }
    SourceType sourceType() const override { return mSourceType; }
    void setSourceType(SourceType sourceType) override;
    void setSourceTypeFromExtension();

    void findReplace();
    void setLineWrap(bool wrap) { setLineWrapMode(wrap ? WidgetWidth : NoWrap); }
    void setFont(const QFont &);
    void setTabSize(int tabSize);
    void setIndentWithSpaces(bool enabled);
    void setShowWhiteSpace(bool enabled);
    bool setCursorPosition(int line, int column);

Q_SIGNALS:
    void fileNameChanged(const QString &fileName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    class LineNumberArea;

    QString tab() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void indentSelection(bool reverse = false);
    void autoIndentNewLine();
    void autoDeindentBrace();
    void removeTrailingSpace();
    void toggleHomePosition(bool shiftHold);
    QString textUnderCursor(bool identifierOnly = false) const;
    QTextCursor findMatchingBrace() const;
    void beginMultiSelection();
    void endMultiSelection();
    bool updateMultiSelection(QKeyEvent *event, bool multiSelectionModifierHold);
    void markOccurrences(QString text, QTextDocument::FindFlags =
        QTextDocument::FindCaseSensitively | QTextDocument::FindWholeWords);
    void handleCursorPositionChanged();
    void handleTextChanged();
    void updateViewportMargins();
    void updateExtraSelections();
    void updateLineNumberArea(const QRect&, int);
    void updateCompleterPopup(const QString &prefix, bool show);
    void insertCompletion(const QString &completion);
    void findReplaceAction(FindReplaceBar::Action action, QString find,
        QString replace, QTextDocument::FindFlags flags);
    void updateColors(bool darkTheme);
    void updateSyntaxHighlighting();

    QString mFileName;
    FindReplaceBar &mFindReplaceBar;
    SourceType mSourceType{ SourceType::PlainText };
    QSyntaxHighlighter *mHighlighter{ };
    QCompleter *mCompleter{ };
    LineNumberArea *mLineNumberArea{ };
    QTextCharFormat mCurrentLineFormat;
    QTextCharFormat mOccurrencesFormat;
    QTextCharFormat mMultiSelectionFormat;
    QList<QTextCursor> mMatchingBraces;
    QList<QTextCursor> mMarkedOccurrences;
    QList<QTextCursor> mMultiSelections;
    QTextCursor mMultiEditCursor;
    QColor mLineNumberColor;
    int mTabSize{ };
    bool mIndentWithSpaces{ };
};

#endif // SOURCEEDITOR_H
