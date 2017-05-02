#include "SourceEditor.h"
#include "SourceFile.h"
#include "Singletons.h"
#include "SourceEditorSettings.h"
#include "FindReplaceBar.h"
#include "render/ShaderCompiler.h"
#include <QSyntaxHighlighter>
#include <QCompleter>
#include <QTextCharFormat>
#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QAction>
#include <QRegularExpression>
#include <QAbstractItemView>
#include <QScrollBar>

class SourceEditor::LineNumberArea : public QWidget
{
public:
    LineNumberArea(SourceEditor *editor) : QWidget(editor), mEditor(editor)
    {
    }

    QSize sizeHint() const override
    {
        return QSize(mEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        mEditor->lineNumberAreaPaintEvent(event);
    }

private:
    SourceEditor *mEditor;
};

SourceEditor::SourceEditor(SourceFilePtr file,
    QSyntaxHighlighter *highlighter, QCompleter *completer, QWidget *parent)
    : QPlainTextEdit(parent)
    , mFile(std::move(file))
    , mHighlighter(highlighter)
    , mCompleter(completer)
    , mLineNumberArea(new LineNumberArea(this))
{
    setDocument(&mFile->document());

    if (mCompleter) {
        mCompleter->setWidget(this);
        mCompleter->setCompletionMode(QCompleter::PopupCompletion);
        mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        using Overload = void(QCompleter::*)(const QString &);
        connect(mCompleter, static_cast<Overload>(&QCompleter::activated),
            this, &SourceEditor::insertCompletion);
    }

    connect(this, &SourceEditor::blockCountChanged,
        this, &SourceEditor::updateViewportMargins);
    connect(this, &SourceEditor::updateRequest,
        this, &SourceEditor::updateLineNumberArea);
    connect(this, &SourceEditor::cursorPositionChanged,
        this, &SourceEditor::updateExtraSelections);
    connect(&Singletons::findReplaceBar(), &FindReplaceBar::action,
        this, &SourceEditor::findReplaceAction);

    mCurrentLineFormat.setProperty(QTextFormat::FullWidthSelection, true);
    mCurrentLineFormat.setBackground(palette().base().color().darker(105));
    mOccurrencesFormat.setBackground(palette().highlight().color().lighter(200));

    mLineNumberColor = palette().window().color().darker(150);

    auto& settings = Singletons::sourceEditorSettings();
    setFont(settings.font());
    setTabSize(settings.tabSize());
    setLineWrap(settings.lineWrap());
    setAutoIndentation(settings.autoIndentation());
    setIndentWithSpaces(settings.indentWithSpaces());

    connect(&settings, &SourceEditorSettings::tabSizeChanged,
        this, &SourceEditor::setTabSize);
    connect(&settings, &SourceEditorSettings::fontChanged,
        this, &SourceEditor::setFont);
    connect(&settings, &SourceEditorSettings::lineWrapChanged,
        this, &SourceEditor::setLineWrap);
    connect(&settings, &SourceEditorSettings::autoIndentationChanged,
        this, &SourceEditor::setAutoIndentation);
    connect(&settings, &SourceEditorSettings::indentWithSpacesChanged,
        this, &SourceEditor::setIndentWithSpaces);

    updateViewportMargins();
    updateExtraSelections();
    setPlainText(mFile->toPlainText());
    refreshShaderCompiler();
}

SourceEditor::~SourceEditor()
{
    setDocument(nullptr);
    mFile.reset();

    if (Singletons::findReplaceBar().target() == this)
        Singletons::findReplaceBar().resetTarget();
}

QList<QMetaObject::Connection> SourceEditor::connectEditActions(
    const EditActions &actions)
{
    actions.undo->setEnabled(document()->isUndoAvailable());
    actions.redo->setEnabled(document()->isRedoAvailable());
    actions.cut->setEnabled(textCursor().hasSelection());
    actions.copy->setEnabled(textCursor().hasSelection());
    actions.paste->setEnabled(canPaste());
    actions.delete_->setEnabled(textCursor().hasSelection());
    actions.selectAll->setEnabled(true);
    actions.findReplace->setEnabled(true);

    auto c = QList<QMetaObject::Connection>();
    c += connect(actions.undo, &QAction::triggered, this, &SourceEditor::undo);
    c += connect(actions.redo, &QAction::triggered, this, &SourceEditor::redo);
    c += connect(actions.cut, &QAction::triggered, this, &SourceEditor::cut);
    c += connect(actions.copy, &QAction::triggered, this, &SourceEditor::copy);
    c += connect(actions.paste, &QAction::triggered, this, &SourceEditor::paste);
    c += connect(actions.delete_, &QAction::triggered, [this]() { textCursor().removeSelectedText(); });
    c += connect(actions.selectAll, &QAction::triggered, this, &SourceEditor::selectAll);
    c += connect(actions.findReplace, &QAction::triggered, this, &SourceEditor::findReplace);

    c += connect(this, &SourceEditor::undoAvailable, actions.undo, &QAction::setEnabled);
    c += connect(this, &SourceEditor::redoAvailable, actions.redo, &QAction::setEnabled);
    c += connect(this, &SourceEditor::copyAvailable, actions.cut, &QAction::setEnabled);
    c += connect(this, &SourceEditor::copyAvailable, actions.copy, &QAction::setEnabled);
    c += connect(this, &SourceEditor::copyAvailable, actions.delete_, &QAction::setEnabled);
    c += connect(QApplication::clipboard(), &QClipboard::changed,
        [this, actions]() { actions.paste->setEnabled(canPaste()); });

    actions.windowFileName->setText(mFile->fileName());
    actions.windowFileName->setEnabled(mFile->document().isModified());
    c += connect(mFile.data(), &SourceFile::fileNameChanged,
        actions.windowFileName, &QAction::setText);
    c += connect(&mFile->document(), &QTextDocument::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    return c;
}

void SourceEditor::findReplace()
{
    Singletons::findReplaceBar().setTarget(this, textUnderCursor());
}

void SourceEditor::setFont(const QFont &font)
{
    QPlainTextEdit::setFont(font);
    setTabSize(mTabSize);
}

void SourceEditor::setTabSize(int tabSize)
{
    mTabSize = tabSize;
    auto width = QFontMetrics(font()).width(QString(tabSize, QChar::Space));
    setTabStopWidth(width);
}

void SourceEditor::setIndentWithSpaces(bool enabled)
{
    mIndentWithSpaces = enabled;
}

void SourceEditor::setAutoIndentation(bool enabled)
{
    mAutoIndentation = enabled;
}

bool SourceEditor::setCursorPosition(int line, int column)
{
    if (line < 0)
        return false;

    auto cursor = textCursor();
    auto block = document()->findBlockByLineNumber(line - 1);
    cursor.setPosition(block.position());
    for (auto i = 0; i < column - 1; i++)
        cursor.movePosition(QTextCursor::Right);
    setTextCursor(cursor);
    ensureCursorVisible();
    setFocus();
    return true;
}

void SourceEditor::refreshShaderCompiler()
{
    // TODO: automatic shader compiling disabled
#if 0
    if (!mShaderCompiler || mShaderCompiler->fileName() != mFile->fileName())
        mShaderCompiler.reset(new ShaderCompiler(mFile->fileName()));
    mShaderCompiler->update();
#endif
}

int SourceEditor::lineNumberAreaWidth()
{
    auto digits = 1;
    auto max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return fontMetrics().width(QLatin1Char('9')) * digits;
}

void SourceEditor::updateViewportMargins()
{
    setViewportMargins(lineNumberAreaWidth() + 6, 0, 0, 0);
}

void SourceEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        mLineNumberArea->scroll(0, dy);
    else
        mLineNumberArea->update(0, rect.y(),
            mLineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateViewportMargins();
}

void SourceEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    const auto rect = contentsRect();
    mLineNumberArea->setGeometry(QRect(rect.left(), rect.top(),
        lineNumberAreaWidth() + 3, rect.height()));
}

void SourceEditor::focusInEvent(QFocusEvent *event)
{
    updateExtraSelections();

    QPlainTextEdit::focusInEvent(event);
}

void SourceEditor::focusOutEvent(QFocusEvent *event)
{
    setExtraSelections({ });

    QPlainTextEdit::focusOutEvent(event);
}

QString SourceEditor::tab() const
{
    return (mIndentWithSpaces ?
            QString(mTabSize, QChar::Space) :
            QString(QChar::Tabulation));
}

void SourceEditor::indentSelection(bool reverse)
{
    const auto newParagraph = QString(QChar(QChar::ParagraphSeparator));
    const auto tabSelector = QString("(\\t| {1,%1})").arg(mTabSize);
    auto cursor = textCursor();
    cursor.beginEditBlock();
    if (!cursor.hasSelection()) {
        if (reverse) {
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            auto text = cursor.selectedText();
            text.remove(QRegularExpression(tabSelector + "$"));
            cursor.insertText(text);
        }
        else {
            cursor.insertText(tab());
        }
    }
    else {
        auto text = cursor.selectedText();
        if (reverse) {
            text.remove(QRegularExpression("^" + tabSelector));
            text.replace(QRegularExpression(newParagraph + tabSelector), "\n");
        }
        else {
            text.replace(QChar(QChar::ParagraphSeparator), "\n" + tab());
            text.prepend(tab());
        }
        cursor.insertText(text);

        cursor.setPosition(cursor.position() - text.size(), QTextCursor::KeepAnchor);
        setTextCursor(cursor);
    }
    cursor.endEditBlock();
    ensureCursorVisible();
}

void SourceEditor::autoIndentNewLine()
{
    auto cursor = textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection())
        cursor.removeSelectedText();

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    auto line = cursor.selectedText();
    auto spaces = QString(line).replace(QRegularExpression("^(\\s*).*"), "\\1");
    if (line.trimmed().endsWith('{'))
        spaces += tab();

    cursor.insertText(line + "\n" + spaces);
    cursor.endEditBlock();
    ensureCursorVisible();
}

void SourceEditor::autoDeindentBrace()
{
    indentSelection(true);
    auto cursor = textCursor();
    cursor.joinPreviousEditBlock();
    cursor.insertText("}");
    cursor.endEditBlock();
    ensureCursorVisible();
}

void SourceEditor::keyPressEvent(QKeyEvent *event)
{
    if (mCompleter && mCompleter->popup()->isVisible()) {
       // The following keys are forwarded by the completer to the widget
       switch (event->key()) {
           case Qt::Key_Enter:
           case Qt::Key_Return:
           case Qt::Key_Escape:
           case Qt::Key_Tab:
           case Qt::Key_Backtab:
                event->ignore();
                return;
           default:
               break;
       }
    }

    auto ctrlHold = (event->modifiers() & Qt::ControlModifier);
    if (ctrlHold && event->key() == Qt::Key_F) {
        findReplace();
    }
    else if (mAutoIndentation && event->key() == Qt::Key_Escape) {
        Singletons::findReplaceBar().cancel();
    }
    else if (mAutoIndentation && event->key() == Qt::Key_Return) {
        autoIndentNewLine();
    }
    else if (mAutoIndentation && event->key() == Qt::Key_BraceRight) {
        autoDeindentBrace();
    }
    else if (event->key() == Qt::Key_Tab) {
        indentSelection();
    }
    else if (event->key() == Qt::Key_Backtab) {
        indentSelection(true);
    }
    else {
        auto prevCharCount = document()->characterCount();

        QPlainTextEdit::keyPressEvent(event);

        const auto hitCtrlOrShift = event->modifiers() &
            (Qt::ControlModifier | Qt::ShiftModifier);
        if (hitCtrlOrShift && event->text().isEmpty())
            return;

        auto prefix = textUnderCursor();
        auto textEntered = (prevCharCount < document()->characterCount());
        auto hitCtrlSpace = ((event->modifiers() & Qt::ControlModifier) &&
            event->key() == Qt::Key_Space);

        auto show = ((textEntered && prefix.size() >= 3) || hitCtrlSpace);
        updateCompleterPopup(prefix, show);
    }
}

void SourceEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        auto font = this->font();
        font.setPointSize(font.pointSize() + (event->delta() > 0 ? 1 : -1));
        Singletons::sourceEditorSettings().setFont(font);
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void SourceEditor::updateExtraSelections()
{
    auto selections = QList<QTextEdit::ExtraSelection>();
    if (hasFocus()) {
        auto cursor = textCursor();
        cursor.clearSelection();
        selections.append({  cursor, mCurrentLineFormat });
    }

    for (auto& occurrence : mMarkedOccurrences)
        selections.append({ occurrence, mOccurrencesFormat });

    setExtraSelections(selections);
}

void SourceEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter{ mLineNumberArea };
    painter.setPen(mLineNumberColor);
    painter.setFont(font());

    auto block = firstVisibleBlock();
    auto blockNumber = block.blockNumber();
    auto top = static_cast<int>(blockBoundingGeometry(block)
        .translated(contentOffset()).top());
    auto bottom = top + static_cast<int>(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top())
            painter.drawText(0, top, mLineNumberArea->width(),
                fontMetrics().height(), Qt::AlignRight,
                QString::number(blockNumber + 1));
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

QString SourceEditor::textUnderCursor() const
{
    auto cursor = textCursor();
    if (cursor.selectedText().isEmpty()) {
        cursor.movePosition(QTextCursor::Left);
        cursor.select(QTextCursor::WordUnderCursor);
    }
    return cursor.selectedText();
}

void SourceEditor::updateCompleterPopup(const QString &prefix, bool show)
{
    if (!mCompleter)
        return;

    if (show) {
        mCompleter->setCompletionPrefix(prefix);
        auto alreadyComplete =
          (mCompleter->currentCompletion() == prefix &&
           mCompleter->completionCount() == 1);

        if (!alreadyComplete) {
            mCompleter->popup()->setCurrentIndex(
                mCompleter->completionModel()->index(0, 0));
            auto rect = cursorRect();
            rect.setWidth(mCompleter->popup()->sizeHintForColumn(0)
                + mCompleter->popup()->verticalScrollBar()->sizeHint().width());
            mCompleter->complete(rect);
            return;
        }
    }
    mCompleter->popup()->hide();
}

void SourceEditor::insertCompletion(const QString &completion)
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    setTextCursor(cursor);
}

void SourceEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    markOccurrences(textUnderCursor());
    QPlainTextEdit::mouseDoubleClickEvent(event);
}

void SourceEditor::markOccurrences(QString text, QTextDocument::FindFlags flags)
{
    mMarkedOccurrences.clear();
    if (!text.isEmpty()) {
        auto cursor = textCursor();
        cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        for (auto it = document()->find(text, cursor, flags);
                !it.isNull(); it = document()->find(text, it, flags))
            mMarkedOccurrences.append(it);
    }
    updateExtraSelections();
}

void SourceEditor::findReplaceAction(FindReplaceBar::Action action,
    QString find, QString replace, QTextDocument::FindFlags flags)
{
    if (Singletons::findReplaceBar().target() != this)
        return;

    markOccurrences(find, flags & (~QTextDocument::FindBackward));

    if (action == FindReplaceBar::ReplaceAll) {
        auto cursor = textCursor();
        cursor.beginEditBlock();

        for (const auto& occurrence : mMarkedOccurrences) {
            cursor.setPosition(occurrence.anchor());
            cursor.setPosition(occurrence.position(), QTextCursor::KeepAnchor);
            cursor.insertText(replace);
        }
        setTextCursor(cursor);
        cursor.endEditBlock();
        return;
    }

    if (action == FindReplaceBar::Replace) {
        auto cursor = textCursor();
        if (!cursor.selectedText().compare(find, Qt::CaseInsensitive)) {
            cursor.insertText(replace);
            setTextCursor(cursor);
        }
    }

    auto cursor = textCursor();
    if (action == FindReplaceBar::FindTextChanged)
        cursor.movePosition(QTextCursor::PreviousWord);

    cursor = document()->find(find, cursor, flags);
    if (cursor.isNull()) {
        cursor = textCursor();
        auto start = ((flags & QTextDocument::FindBackward) ?
            QTextCursor::End : QTextCursor::Start);
        cursor.movePosition(start, QTextCursor::MoveAnchor);
        cursor = document()->find(find, cursor, flags);
    }

    if (cursor.isNull()) {
        cursor = textCursor();
        cursor.clearSelection();
    }

    setTextCursor(cursor);
    centerCursor();
}
