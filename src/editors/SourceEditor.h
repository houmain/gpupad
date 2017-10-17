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
    enum SourceType
    {
        None,
        PlainText,
        VertexShader,
        FragmentShader,
        GeometryShader,
        TesselationControl,
        TesselationEvaluation,
        ComputeShader,
        JavaScript,
    };
    Q_ENUM(SourceType)

    static bool load(const QString &fileName, QString *source);

    explicit SourceEditor(QString fileName, QWidget *parent = nullptr);
    ~SourceEditor();

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    int tabifyGroup() override { return 0; }
    QString source() const { return toPlainText(); }
    SourceType sourceType() const { return mSourceType; }
    void setSourceTypeFromExtension();
    void setSourceType(SourceType sourceType);

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
    void toggleHomePosition(bool shiftHold);
    QString textUnderCursor(bool identifierOnly = false) const;
    void markOccurrences(QString text, QTextDocument::FindFlags =
        QTextDocument::FindCaseSensitively | QTextDocument::FindWholeWords);
    void handleTextChanged();
    void updateSyntaxHighlighting();

    QString mFileName;
    SourceType mSourceType{ PlainText };
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
