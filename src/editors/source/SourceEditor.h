#pragma once

#include "FindReplaceBar.h"
#include "MultiTextCursors.h"
#include "SourceType.h"
#include "editors/IEditor.h"
#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;
class QCompleter;
class SyntaxHighlighter;
class Completer;
class SourceEditorToolBar;
class Theme;

class SourceEditor final : public QPlainTextEdit, public IEditor
{
    Q_OBJECT
public:
    SourceEditor(QString fileName, SourceEditorToolBar *editorToolbar,
        FindReplaceBar *findReplaceBar, QWidget *parent = nullptr);
    ~SourceEditor() override;

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    int tabifyGroup() const override { return 0; }
    void setModified() override;
    void replace(QString source, bool emitFileChanged = true);
    void cut();
    void copy();
    void paste();

    void findReplace();
    void setLineWrap(bool wrap);
    void disableLineWrap();
    void restoreLineWrap();
    void setFont(const QFont &);
    void setTabSize(int tabSize);
    void setIndentWithSpaces(bool enabled);
    void setShowWhiteSpace(bool enabled);
    void setCursorPosition(int line, int column);
    QString source() const { return toPlainText(); }
    SourceType sourceType() const { return mSourceType; }
    void setSourceType(SourceType sourceType);
    void deduceSourceType();
    void restoreNavigationPosition(const QString &position);

Q_SIGNALS:
    void fileNameChanged(const QString &fileName);
    void navigationPositionChanged(const QString &position, bool replace);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

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
    void handleCursorPositionChanged();
    void handleTextChanged();
    void updateViewportMargins();
    void updateExtraSelections();
    void updateLineNumberArea(const QRect &, int);
    void updateCompleterPopup(const QString &prefix, bool show);
    QString generateCurrentScopeSource() const;
    void insertCompletion(const QString &completion);
    void updateColors(const Theme &theme);
    void updateSyntaxHighlighting();
    void updateEditorToolBar();
    void emitNavigationPositionChanged();
    QTextCursor find(const QString &subString, int from, int to,
        QTextDocument::FindFlags options = QTextDocument::FindFlags()) const;
    void findReplaceAction(FindReplaceBar::Action action, QString find,
        QString replace, QTextDocument::FindFlags flags);
    void markOccurrences(QString text,
        QTextDocument::FindFlags = QTextDocument::FindCaseSensitively
            | QTextDocument::FindWholeWords);
    void clearMarkedOccurrences();
    void clearFindReplaceRange();
    QTextCursor updateFindReplaceRange();

    SourceEditorToolBar &mEditorToolBar;
    QString mFileName;
    FindReplaceBar &mFindReplaceBar;
    SourceType mSourceType{ SourceType::PlainText };
    SyntaxHighlighter *mHighlighter{};
    Completer *mCompleter{};
    LineNumberArea *mLineNumberArea{};
    QTextCharFormat mCurrentLineFormat;
    QTextCharFormat mOccurrencesFormat;
    QTextCharFormat mFindReplaceRangeFormat;
    QTextCharFormat mMultiSelectionFormat;
    QList<QTextCursor> mMatchingBraces;
    QList<QTextCursor> mMarkedOccurrences;
    QRect mMarkedOccurrencesRect;
    QString mMarkedOccurrencesString;
    QTextDocument::FindFlags mMarkedOccurrencesFindFlags{};
    MultiTextCursors mMultiTextCursors;
    QTextCursor mFindReplaceRange;
    QColor mLineNumberColor;
    QColor mCurrenLineNumberColor;
    int mTabSize{};
    int mInitialCursorWidth{};
    bool mIndentWithSpaces{};
    int mUpdatedCompleterInBlock{};
    QString mPrevNavigationPosition;
    LineWrapMode mSetLineWrapMode{};
};
