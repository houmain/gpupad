#include "SourceEditor.h"
#include "SourceEditorToolBar.h"
#include "Singletons.h"
#include "FileCache.h"
#include "Settings.h"
#include "FindReplaceBar.h"
#include "FileDialog.h"
#include "SyntaxHighlighter.h"
#include "Completer.h"
#include "getMousePosition.h"
#include <QCompleter>
#include <QTextCharFormat>
#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QAction>
#include <QRegularExpression>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QTextStream>
#include <QMenu>
#include <QMimeData>

class SourceEditor::LineNumberArea final : public QWidget
{
private:
    SourceEditor &mEditor;
    int mSelectionStart{ -1 };

public:
    static const auto margin = 6;

    LineNumberArea(SourceEditor *editor)
        : QWidget(editor)
        , mEditor(*editor) 
    {
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);
        setMouseTracking(true);
    }

    QSize sizeHint() const override
    {
        return QSize(mEditor.lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        mEditor.lineNumberAreaPaintEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const auto pos = getMousePosition(event);
        if (event->button() == Qt::LeftButton && pos.x() < width() - margin / 2) {
            auto cursor = mEditor.cursorForPosition({ 0, pos.y() });
            const auto position = cursor.position();
            cursor.movePosition(QTextCursor::StartOfBlock);
            mSelectionStart = cursor.position();
            cursor.movePosition(QTextCursor::NextBlock);
            cursor.setPosition(position, QTextCursor::KeepAnchor);
            mEditor.setTextCursor(cursor);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const auto pos = getMousePosition(event);
        auto cursor = mEditor.cursorForPosition({ 0, pos.y() });
        if (mSelectionStart >= 0) {
            const auto position = cursor.position();
            cursor.setPosition(mSelectionStart);
            if (position <= mSelectionStart) {
                cursor.movePosition(QTextCursor::NextBlock);
                cursor.setPosition(position, QTextCursor::KeepAnchor);
            } 
            else {
                cursor.setPosition(position, QTextCursor::KeepAnchor);
                cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
            }
            mEditor.setTextCursor(cursor);
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        mSelectionStart = -1;
        QWidget::mouseReleaseEvent(event);
    }
};

SourceEditor::SourceEditor(QString fileName
    , SourceEditorToolBar* editorToolbar
    , FindReplaceBar *findReplaceBar
    , QWidget *parent)
    : QPlainTextEdit(parent)
    , mEditorToolBar(*editorToolbar)
    , mFileName(fileName)
    , mFindReplaceBar(*findReplaceBar)
    , mCompleter(new Completer(this))
    , mLineNumberArea(new LineNumberArea(this))
    , mInitialCursorWidth(1)
{
    mCompleter->setWidget(this);

    connect(this, &SourceEditor::blockCountChanged,
        this, &SourceEditor::updateViewportMargins);
    connect(this, &SourceEditor::updateRequest,
        this, &SourceEditor::updateLineNumberArea);
    connect(this, &SourceEditor::cursorPositionChanged,
        this, &SourceEditor::handleCursorPositionChanged);
    connect(this, &SourceEditor::textChanged,
        this, &SourceEditor::handleTextChanged);
    connect(&mFindReplaceBar, &FindReplaceBar::action,
        this, &SourceEditor::findReplaceAction);
    connect(&mMultiTextCursors, &MultiTextCursors::cursorsChanged,
        this, &SourceEditor::updateExtraSelections);
    connect(&mMultiTextCursors, &MultiTextCursors::cursorChanged,
        this, &SourceEditor::setTextCursor);
    connect(&mMultiTextCursors, &MultiTextCursors::disableLineWrap,
        this, &SourceEditor::disableLineWrap);
    connect(&mMultiTextCursors, &MultiTextCursors::restoreLineWrap,
        this, &SourceEditor::restoreLineWrap);
    connect(mCompleter, qOverload<const QString &>(&QCompleter::activated),
        this, &SourceEditor::insertCompletion);

    const auto &settings = Singletons::settings();
    setFont(settings.font());
    setTabSize(settings.tabSize());
    setLineWrap(settings.lineWrap());
    setShowWhiteSpace(settings.showWhiteSpace());
    setIndentWithSpaces(settings.indentWithSpaces());
    setCursorWidth(mInitialCursorWidth);

    connect(&settings, &Settings::tabSizeChanged,
        this, &SourceEditor::setTabSize);
    connect(&settings, &Settings::fontChanged,
        this, &SourceEditor::setFont);
    connect(&settings, &Settings::lineWrapChanged,
        this, &SourceEditor::setLineWrap);
    connect(&settings, &Settings::showWhiteSpaceChanged,
        this, &SourceEditor::setShowWhiteSpace);
    connect(&settings, &Settings::indentWithSpacesChanged,
        this, &SourceEditor::setIndentWithSpaces);
    connect(&settings, &Settings::darkThemeChanged,
        this, &SourceEditor::updateColors);
    connect(&settings, &Settings::darkThemeChanged,
        this, &SourceEditor::updateSyntaxHighlighting);

    updateViewportMargins();
    updateColors(settings.darkTheme());
}

SourceEditor::~SourceEditor()
{
    disconnect(this, &SourceEditor::textChanged,
        this, &SourceEditor::handleTextChanged);
    disconnect(this, &SourceEditor::cursorPositionChanged,
        this, &SourceEditor::handleCursorPositionChanged);

    handleTextChanged();
    setDocument(nullptr);

    if (mFindReplaceBar.target() == this)
        mFindReplaceBar.resetTarget();
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

    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(document()->isModified());
    c += connect(this, &SourceEditor::fileNameChanged,
        actions.windowFileName, &QAction::setText);
    c += connect(document(), &QTextDocument::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    updateEditorToolBar();

    const auto& settings = Singletons::settings();
    c += connect(&mEditorToolBar, &SourceEditorToolBar::sourceTypeChanged, 
        this, &SourceEditor::setSourceType);
    c += connect(&mEditorToolBar, &SourceEditorToolBar::lineWrapChanged, 
        &settings, &Settings::setLineWrap);

    return c;
}

void SourceEditor::setFileName(QString fileName)
{
    if (mFileName != fileName) {
        mFileName = fileName;
        Q_EMIT fileNameChanged(mFileName);
    }
}

bool SourceEditor::load()
{
    auto source = QString();
    if (!Singletons::fileCache().getSource(mFileName, &source))
        return false;

    replace(source);
    document()->setModified(false);
    return true;
}

bool SourceEditor::save()
{
    QFile file(fileName());
    if (!file.open(QFile::WriteOnly | QFile::Text))
        return false;

    file.write(document()->toPlainText().toUtf8());
    document()->setModified(false);
    deduceSourceType();
    return true;
}

void SourceEditor::replace(QString source)
{
    const auto current = document()->toPlainText();
    const auto initial = current.isEmpty() && !document()->isUndoAvailable();
    if (initial)
        document()->setUndoRedoEnabled(false);

    const auto firstDiff = [&]() {
        const auto n = static_cast<int>(std::min(source.length(), current.length()));
        for (auto i = 0; i < n; i++)
            if (source[i] != current[i])
                return i;
        return n;
    }();

    if (firstDiff != source.length() || 
        firstDiff != current.length()) {

        const auto [lastDiffSource, lastDiffCurrent] = [&]() -> std::pair<int, int>{
            for (auto i = source.length() - 1, 
                      j = current.length() - 1; ; --i, --j)
                if (i < firstDiff || j < firstDiff || source[i] != current[j])
                    return { i + 1, j + 1 };
            return { };
        }();

        auto cursor = textCursor();
        const auto position = cursor.position();
        const auto anchor = cursor.anchor();
        const auto scrollPosition = verticalScrollBar()->sliderPosition();
        const auto scrollToEnd = 
            (!initial && scrollPosition == verticalScrollBar()->maximum());
        cursor.beginEditBlock();
        cursor.setPosition(firstDiff);
        cursor.setPosition(lastDiffCurrent, QTextCursor::KeepAnchor);
        const auto diff = QString(source.mid(firstDiff, lastDiffSource - firstDiff));
        cursor.insertText(diff);
        cursor.setPosition(std::min(anchor, firstDiff));
        cursor.setPosition(std::min(position, firstDiff), QTextCursor::KeepAnchor);
        cursor.endEditBlock();
        setTextCursor(cursor);
        verticalScrollBar()->setSliderPosition(scrollToEnd ? 
            verticalScrollBar()->maximum() : scrollPosition);
    }

    document()->setUndoRedoEnabled(true);
    if (initial) {
        mSourceType = SourceType::Generic;
        deduceSourceType();
    }
}

void SourceEditor::cut()
{
    if (!mMultiTextCursors.cut())
        QPlainTextEdit::cut();
}

void SourceEditor::copy()
{
    if (!mMultiTextCursors.copy())
        QPlainTextEdit::copy();
}

void SourceEditor::paste()
{
    if (!mMultiTextCursors.paste())
        QPlainTextEdit::paste();
}

void SourceEditor::deduceSourceType()
{
    const auto extension = QFileInfo(mFileName).suffix();
    setSourceType(::deduceSourceType(sourceType(), extension, toPlainText()));
}

void SourceEditor::emitNavigationPositionChanged()
{
    const auto cursor = textCursor();
    const auto block = cursor.block();
    const auto offset = cursor.position() - block.position();
    const auto position = QString("%1;%2").arg(block.blockNumber()).arg(offset);

    // update previous when block number did not change
    const auto update = (mPrevNavigationPosition.startsWith(
      QString("%1;").arg(block.blockNumber())));

    Q_EMIT navigationPositionChanged(position, update);
    mPrevNavigationPosition = position;
}

void SourceEditor::restoreNavigationPosition(const QString &position)
{
    const auto semicolon = position.indexOf(";");
    const auto blockNumber = position.left(semicolon).toInt();
    const auto offset = position.mid(semicolon + 1).toInt();

    disableLineWrap();

    auto cursor = textCursor();
    const auto block = document()->findBlockByNumber(blockNumber);
    cursor.setPosition(block.position() + offset);
    setTextCursor(cursor);

    restoreLineWrap();
}

void SourceEditor::setSourceType(SourceType sourceType)
{
    if (!mHighlighter || mSourceType != sourceType) {
        mSourceType = sourceType;
        updateSyntaxHighlighting();
    }
}

void SourceEditor::updateColors(bool darkTheme)
{
    const auto pal = qApp->palette();
    mCurrentLineFormat.setProperty(QTextFormat::FullWidthSelection, true);
    mCurrentLineFormat.setBackground(pal.base().color().lighter(darkTheme ? 120 : 95));

    mFindReplaceRangeFormat.setBackground(pal.base().color().lighter(darkTheme ? 170 : 90));

    mOccurrencesFormat.setForeground(pal.text().color().lighter(darkTheme ? 140 : 80));
    mOccurrencesFormat.setBackground(pal.base().color().lighter(darkTheme ? 220 : 80));

    mMultiSelectionFormat.setForeground(pal.highlightedText());
    mMultiSelectionFormat.setBackground(pal.highlight());

    mLineNumberColor = pal.base().color().darker(darkTheme ? 40 : 140);
    mCurrenLineNumberColor = mLineNumberColor.darker(darkTheme ? 60 : 120);

    updateExtraSelections();
}

void SourceEditor::updateSyntaxHighlighting()
{
    const auto limit = (1 << 17); // 128k
    const auto disabled = (document()->characterCount() > limit);

    delete mHighlighter;
    mHighlighter = nullptr;
    if (mSourceType == SourceType::PlainText || disabled)
        return;

    const auto &settings = Singletons::settings();
    mHighlighter = new SyntaxHighlighter(
        mSourceType, settings.darkTheme(), 
        settings.showWhiteSpace(), this);
    mHighlighter->setDocument(document());
    mCompleter->setStrings(mHighlighter->completerStrings());
}

void SourceEditor::updateEditorToolBar()
{
    const auto &settings = Singletons::settings();
    mEditorToolBar.setSourceType(mSourceType);
    mEditorToolBar.setLineWrap(settings.lineWrap());
}

void SourceEditor::findReplace()
{
    mMultiTextCursors.clear();
    mFindReplaceBar.setTarget(this);

    if (auto text = textUnderCursor(); 
        !text.isEmpty() && !text.contains(QChar::ParagraphSeparator))
        mFindReplaceBar.setText(text);

    if (mFindReplaceBar.isVisible())
        mFindReplaceBar.focus();
    updateExtraSelections();
}

void SourceEditor::setLineWrap(bool wrap)
{
    const auto scrollPosition = verticalScrollBar()->sliderPosition();
    mSetLineWrapMode = (wrap ? WidgetWidth : NoWrap);
    setLineWrapMode(mSetLineWrapMode);
    verticalScrollBar()->setSliderPosition(scrollPosition);
}

void SourceEditor::disableLineWrap()
{
    if (mSetLineWrapMode != NoWrap)
        setLineWrapMode(NoWrap);
}

void SourceEditor::restoreLineWrap()
{
    if (mSetLineWrapMode != NoWrap)
        setLineWrapMode(mSetLineWrapMode);
}

void SourceEditor::setFont(const QFont &font)
{
    QPlainTextEdit::setFont(font);
    mCompleter->popup()->setFont(font);
    setTabSize(mTabSize);
}

void SourceEditor::setTabSize(int tabSize)
{
    mTabSize = tabSize;
    setTabStopDistance(fontMetrics().horizontalAdvance(
        QString(tabSize, QChar::Space)));
    mMultiTextCursors.setTab(tab());
}

void SourceEditor::setIndentWithSpaces(bool enabled)
{
    mIndentWithSpaces = enabled;
    mMultiTextCursors.setTab(tab());
}

void SourceEditor::setShowWhiteSpace(bool enabled)
{
    auto options = document()->defaultTextOption();
    options.setFlags(enabled
        ? options.flags() | QTextOption::ShowTabsAndSpaces
        : options.flags() & ~QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(options);
    if (mHighlighter)
        updateSyntaxHighlighting();
}

void SourceEditor::setCursorPosition(int line, int column)
{
    disableLineWrap();
    auto cursor = textCursor();
    auto block = document()->findBlockByLineNumber(line - 1);
    cursor.setPosition(block.position());
    for (auto i = 0; i < column - 1; i++)
        cursor.movePosition(QTextCursor::Right);
    setTextCursor(cursor);
    restoreLineWrap();

    ensureCursorVisible();
    setFocus();
    emitNavigationPositionChanged();
}

int SourceEditor::lineNumberAreaWidth()
{
    auto digits = 1;
    auto max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return fontMetrics().horizontalAdvance(QString(digits, '9')) + 
        2 * LineNumberArea::margin;
}

void SourceEditor::updateViewportMargins()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
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

void SourceEditor::paintEvent(QPaintEvent *event)
{
    markOccurrences(mMarkedOccurrencesString, mMarkedOccurrencesFindFlags);

    QPlainTextEdit::paintEvent(event);

    if (!mMultiTextCursors.cursors().empty()) {
        QPainter painter(viewport());
        for (const auto &selection : mMultiTextCursors.cursors()) {
            const auto rect = cursorRect(selection);
            painter.fillRect(rect.x(), rect.y(), mInitialCursorWidth,
                rect.height(), QPalette().text());
        }
    }
}

void SourceEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    const auto rect = contentsRect();
    mLineNumberArea->setGeometry(QRect(rect.left(), rect.top(),
        lineNumberAreaWidth(), rect.height()));
}

void SourceEditor::focusInEvent(QFocusEvent *event)
{
    if (mFindReplaceBar.isVisible())
        mFindReplaceBar.setTarget(this);

    QPlainTextEdit::focusInEvent(event);
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
    if (!cursor.hasSelection() || cursor.selectedText().indexOf(newParagraph) < 0) {
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
        auto begin = std::min(cursor.selectionStart(), cursor.selectionEnd());
        auto end = std::max(cursor.selectionStart(), cursor.selectionEnd());
        cursor.setPosition(begin);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        auto text = cursor.selectedText();
        if (reverse) {
            text.remove(QRegularExpression("^" + tabSelector));
            text.replace(QRegularExpression(newParagraph + tabSelector), newParagraph);
        }
        else {
            text.replace(newParagraph, newParagraph + tab());
            text.prepend(tab());
        }
        cursor.insertText(text);
        end = cursor.position();
        cursor.setPosition(end - text.length());
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
    }
    cursor.endEditBlock();
    ensureCursorVisible();
}

void SourceEditor::autoIndentNewLine()
{
    const auto newParagraph = QString(QChar(QChar::ParagraphSeparator));
    auto cursor = textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection())
        cursor.removeSelectedText();

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    auto line = cursor.selectedText();
    while (!line.isEmpty() && (line.back() == ' ' || line.back() == '\t'))
        line.chop(1);
    if (line.isEmpty())
        line = cursor.selectedText();

    static const auto selectSpaces = QRegularExpression("^(\\s*).*");
    auto spaces = QString(line).replace(selectSpaces, "\\1");
    if (line.endsWith('{'))
        spaces += tab();

    cursor.insertText(line + newParagraph + spaces);
    cursor.endEditBlock();
    ensureCursorVisible();
}

void SourceEditor::autoDeindentBrace()
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    if (cursor.selectedText().trimmed().isEmpty())
        indentSelection(true);
    cursor = textCursor();
    cursor.joinPreviousEditBlock();
    cursor.insertText("}");
    cursor.endEditBlock();
    ensureCursorVisible();
}

void SourceEditor::removeTrailingSpace()
{
    auto cursor = textCursor();
    if (!cursor.selectedText().isEmpty())
        return;
    cursor.joinPreviousEditBlock();
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    if (cursor.selectedText().trimmed().isEmpty())
        cursor.removeSelectedText();
    cursor.endEditBlock();
}

void SourceEditor::toggleHomePosition(bool shiftHold)
{
    auto cursor = textCursor();
    const auto initial = cursor.columnNumber();
    const auto moveMode = (shiftHold ?
        QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::StartOfLine, moveMode);
    if (cursor.movePosition(QTextCursor::EndOfWord, moveMode)) {
        // line starts with a word, always move to line start
        cursor.movePosition(QTextCursor::StartOfLine, moveMode);
    }
    else if (!cursor.atBlockEnd()) {
        // line starts with whitespace
        // toggle between line start and end of whitespace
        cursor.movePosition(QTextCursor::NextWord, moveMode);
        if (0 < initial && initial <= cursor.columnNumber())
            cursor.movePosition(QTextCursor::StartOfLine, moveMode);
    }
    setTextCursor(cursor);
}

void SourceEditor::keyPressEvent(QKeyEvent *event)
{
    if (mCompleter->popup()->isVisible()) {
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

    const auto ctrlHold = (event->modifiers() & Qt::ControlModifier);

    if (mMultiTextCursors.handleKeyPressEvent(event, textCursor()))
        return;

    if (ctrlHold && (event->key() == Qt::Key_F || event->key() == Qt::Key_F3)) {
        findReplace();
        if (event->key() == Qt::Key_F3)
            mFindReplaceBar.findNext();
    }
    else if (event->key() == Qt::Key_F3) {
        mMultiTextCursors.clear();
        mFindReplaceBar.setTarget(this);
        if (event->modifiers() & Qt::ShiftModifier)
            mFindReplaceBar.findPrevious();
        else
            mFindReplaceBar.findNext();
    }
    else if (event->key() == Qt::Key_Escape) {
        mFindReplaceBar.cancel();
    }
    else if (event->key() == Qt::Key_Insert && !event->modifiers()) {
        setOverwriteMode(!overwriteMode());
        update();
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        autoIndentNewLine();
        removeTrailingSpace();
    }
    else if (event->key() == Qt::Key_BraceRight) {
        autoDeindentBrace();
    }
    else if (event->key() == Qt::Key_Home) {
        toggleHomePosition(event->modifiers() & Qt::ShiftModifier);
    }
    else if (event->key() == Qt::Key_Tab) {
        indentSelection();
    }
    else if (event->key() == Qt::Key_Backtab) {
        indentSelection(true);
    }
    else if (event->key() == Qt::Key_Backspace || 
             event->key() == Qt::Key_Delete) {
        QPlainTextEdit::keyPressEvent(event);
        removeTrailingSpace();
    }
    else {
        QPlainTextEdit::keyPressEvent(event);

        const auto hitCtrlOrShift = event->modifiers() &
            (Qt::ControlModifier | Qt::ShiftModifier);
        if (hitCtrlOrShift && event->text().isEmpty())
            return;

        const auto prefix = textUnderCursor().trimmed();
        const auto text = event->text();
        const auto textEntered = !text.isEmpty() && 
            (text.at(0).isLetterOrNumber() || text.at(0) == '_');
        const auto hitCtrlSpace = ((event->modifiers() & Qt::ControlModifier) &&
            event->key() == Qt::Key_Space);

        const auto show = ((textEntered && prefix.size() >= 1) || hitCtrlSpace);
        updateCompleterPopup(prefix, show);
    }

    if (!event->text().isEmpty() && event->isAccepted())
        emitNavigationPositionChanged();
}

void SourceEditor::wheelEvent(QWheelEvent *event)
{
    setFocus();

    if (event->modifiers() == Qt::ControlModifier) {
        auto font = this->font();
        font.setPointSize(font.pointSize() + (event->angleDelta().y() > 0 ? 1 : -1));
        Singletons::settings().setFont(font);
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void SourceEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (mMultiTextCursors.handleMouseDoubleClickEvent(event, textCursor()))
        return;
    QPlainTextEdit::mouseDoubleClickEvent(event);
    markOccurrences(textUnderCursor(true));
}

void SourceEditor::mousePressEvent(QMouseEvent *event)
{
    clearFindReplaceRange();
    const auto prevCursor = textCursor();
    mMultiTextCursors.handleBeforeMousePressEvent(event, cursorForPosition(event->pos()));
    QPlainTextEdit::mousePressEvent(event);
    mMultiTextCursors.handleMousePressedEvent(event, textCursor(), prevCursor);
    emitNavigationPositionChanged();
}

void SourceEditor::mouseMoveEvent(QMouseEvent *event)
{
    QPlainTextEdit::mouseMoveEvent(event);
    mMultiTextCursors.handleMouseMoveEvent(event, textCursor());
}

void SourceEditor::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        return event->ignore();

    QPlainTextEdit::dragEnterEvent(event);
}

void SourceEditor::handleCursorPositionChanged()
{  
    auto cursor = textCursor();
    if (auto brace = findMatchingBrace(); !brace.isNull()) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        brace.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        mMatchingBraces.clear();
        mMatchingBraces.append(cursor);
        mMatchingBraces.append(brace);
    }
    else if (!mMatchingBraces.isEmpty()) {
        mMatchingBraces.clear();
    }

    updateExtraSelections();
}

void SourceEditor::handleTextChanged()
{
    clearMarkedOccurrences();
    if (!mFindReplaceBar.replacing())
        clearFindReplaceRange();
    if (document()->isUndoAvailable() || document()->isRedoAvailable())
        Singletons::fileCache().handleEditorFileChanged(mFileName);
}

void SourceEditor::updateExtraSelections()
{
    auto selections = QList<QTextEdit::ExtraSelection>();

    auto cursor = textCursor();
    cursor.clearSelection();
    selections.append({ cursor, mCurrentLineFormat });

    if (mFindReplaceRange.hasSelection())
        selections.append({ mFindReplaceRange, mFindReplaceRangeFormat });

    for (const auto &occurrence : qAsConst(mMatchingBraces))
        selections.append({ occurrence, mOccurrencesFormat });

    for (const auto &occurrence : qAsConst(mMarkedOccurrences))
        selections.append({ occurrence, mOccurrencesFormat });

    for (const auto &selection : mMultiTextCursors.cursors())
        selections.append({ selection, mMultiSelectionFormat });

    setExtraSelections(selections);
    setCursorWidth(mMultiTextCursors.cursors().isEmpty() ?
        mInitialCursorWidth : 0);
}

void SourceEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter{ mLineNumberArea };

    const auto currentBlockNumber = textCursor().block().blockNumber();
    auto block = firstVisibleBlock();
    auto blockNumber = block.blockNumber();
    auto top = static_cast<int>(blockBoundingGeometry(block)
        .translated(contentOffset()).top());
    auto bottom = top + static_cast<int>(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (blockNumber == currentBlockNumber) {
                auto boldFont = font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
                painter.setPen(mCurrenLineNumberColor);
            }
            else {
                painter.setFont(font());
                painter.setPen(mLineNumberColor);
            }
            painter.drawText(-LineNumberArea::margin, top, mLineNumberArea->width(),
                fontMetrics().height(), Qt::AlignRight,
                QString::number(blockNumber + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

QString SourceEditor::textUnderCursor(bool identifierOnly) const
{
    auto cursor = textCursor();
    if (cursor.selectedText().isEmpty() || identifierOnly) {
        cursor.movePosition(QTextCursor::Left);
        cursor.select(QTextCursor::WordUnderCursor);
    }
    auto text = cursor.selectedText();
    if (identifierOnly) {
        static const auto regex = QRegularExpression("[^a-zA-Z0-9_]");
        text = text.replace(regex, "");
    }
    return text;
}

QTextCursor SourceEditor::findMatchingBrace() const
{
    const auto limit = (1 << 12); // 4k
    auto cursor = textCursor();
    auto position = cursor.positionInBlock();
    auto block = cursor.block();
    if (position >= block.text().length())
        return { };
    const auto beginChar = block.text().at(position);

    auto endChar = QChar{ };
    auto direction = 1;
    switch (beginChar.unicode()) {
        case '{': endChar = '}'; break;
        case '[': endChar = ']'; break;
        case '(': endChar = ')'; break;
        case '}': endChar = '{'; direction = -1; break;
        case ']': endChar = '['; direction = -1; break;
        case ')': endChar = '('; direction = -1; break;
        default: return { };
    }
    for (auto level = 0, i = 0; i < limit; position += direction, ++i) {
        while (position < 0) {
            block = block.previous();
            if (!block.isValid())
                return { };
            position += block.text().length();
        }
        while (position >= block.text().length()) {
            position -= block.text().length();
            block = block.next();
            if (!block.isValid())
                return { };
        }
        const auto currentChar = block.text().at(position);
        if (currentChar == beginChar) {
            ++level;
        }
        else if (currentChar == endChar) {
            --level;
            if (level == 0) {
                cursor.setPosition(block.position() + position);
                return cursor;
            }
        }
    }
    return { };
}

void SourceEditor::updateCompleterPopup(const QString &prefix, bool show)
{
    if (show) {
        const auto blockNumber = textCursor().blockNumber();
        if (mUpdatedCompleterInBlock != blockNumber) {
            mCompleter->setContextText(generateCurrentScopeSource());
            mUpdatedCompleterInBlock = blockNumber;
        }

        mCompleter->setCompletionPrefix(prefix);
        const auto alreadyComplete =
          (mCompleter->currentCompletion() == prefix &&
           mCompleter->completionCount() == 1);

        if (!alreadyComplete) {
            mCompleter->popup()->setCurrentIndex(
                mCompleter->completionModel()->index(0, 0));
            auto cursor = textCursor();
            cursor.movePosition(QTextCursor::WordLeft);
            auto rect = cursorRect(cursor);
            rect.adjust(viewportMargins().left(), 4, 0, 4);
            rect.setWidth(mCompleter->popup()->sizeHintForColumn(0)
                + mCompleter->popup()->verticalScrollBar()->sizeHint().width());
            mCompleter->complete(rect);
            return;
        }
    }
    mCompleter->popup()->hide();
}

QString SourceEditor::generateCurrentScopeSource() const
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::PreviousWord);
    cursor.setPosition(0, QTextCursor::KeepAnchor);
    auto text = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');

    const static auto multiLineComment = QRegularExpression(
        "/\\*.*?\\*/", QRegularExpression::MultilineOption);
    const static auto singleLineComment = QRegularExpression("//[^\n]*");
    text.remove(multiLineComment);
    text.remove(singleLineComment);

    auto level = 0;
    auto scopeEnd = -1;
    for (auto i = text.size() - 1; i >= 0; --i) {
      if (text[i] == '}') {
          if (level == 0) {
              scopeEnd = i;
          }
          ++level;
      }
      else if (text[i] == '{') {
          if (level == 1) {
              text = text.remove(i + 1, scopeEnd - i - 1);
              scopeEnd = -1;
          }
          level = std::max(level - 1, 0);
      }
    }
    return text;
}

void SourceEditor::insertCompletion(const QString &completion)
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    setTextCursor(cursor);
}

// manually searching blocks since QTextDocument::find() has no 'to' parameter...
QTextCursor SourceEditor::find(const QString &string, int from, int to, 
    QTextDocument::FindFlags options) const
{
    const auto forward = ((options & QTextDocument::FindBackward) == 0);
    const auto backward = !forward;
    const auto wholeWord = ((options & QTextDocument::FindWholeWords) != 0);
    const auto caseSensitivity = (options & QTextDocument::FindCaseSensitively ?
        Qt::CaseSensitive : Qt::CaseInsensitive);
    if (forward ? from > to : from < to)
        return { };

    auto block = document()->findBlock(from);
    for (;;) {
        if (!block.isValid() ||
            (forward && block.position() >= to) || 
            (backward && block.position() + block.length() <= to))
            return { };

        const auto text = block.text();
        auto offset = std::min(std::max(from - block.position(), 0), 
            static_cast<int>(text.length()) - 1);
        for (; offset >= 0; offset += (forward ? 1 : -1)) {
            offset = (forward ? text.indexOf(string, offset, caseSensitivity) : 
                      text.lastIndexOf(string, offset, caseSensitivity));
            if (offset < 0)
                break;

            const auto begin = block.position() + offset;
            const auto end = begin + string.length();
            if (backward && end > from)
                continue;
            if (forward && end > to)
                return { };
            if (backward && begin < to)
                return { };
            if (wholeWord && 
                offset - 1 >= 0 && text[offset - 1].isLetterOrNumber())
                continue;
            if (wholeWord && 
                offset + string.length() < text.length() && 
                text[offset + string.length()].isLetterOrNumber())
                continue;
                                
            auto cursor = textCursor();
            cursor.setPosition(begin);
            cursor.setPosition(end, QTextCursor::KeepAnchor);
            return cursor;
        }
        block = (forward ? block.next() : block.previous());
    }
}

void SourceEditor::findReplaceAction(FindReplaceBar::Action action,
    QString string, QString replace, QTextDocument::FindFlags flags)
{
    if (mFindReplaceBar.target() != this)
        return;

    if (action == FindReplaceBar::Cancel) {
        clearMarkedOccurrences();
        clearFindReplaceRange();
        return;
    }

    const auto range = updateFindReplaceRange();
    const auto top = std::min(range.anchor(), range.position());
    auto bottom = std::max(range.anchor(), range.position());
    const auto inRange = [&](const QTextCursor &cursor) {
        return !(cursor.isNull() || cursor.position() < top || cursor.position() > bottom);
    };

    if (action == FindReplaceBar::Replace) {
        auto cursor = textCursor();
        if (!cursor.selectedText().compare(string, 
            QTextDocument::FindCaseSensitively ? 
                Qt::CaseSensitive : Qt::CaseInsensitive)) {
            cursor.insertText(replace);
        }
        action = FindReplaceBar::Find;
        mMarkedOccurrencesString.clear();
    }
    else if (action == FindReplaceBar::ReplaceAll) {
        if (!string.isEmpty()) {
            Q_ASSERT(!(flags & QTextDocument::FindBackward));
            auto cursor = find(string, top, bottom, flags);
            if (inRange(cursor)) {
                cursor.beginEditBlock();
                while (inRange(cursor)) {
                    const auto delta = replace.length() - string.length();
                    const auto position = cursor.position() + delta;
                    cursor.insertText(replace);
                    bottom += delta;
                    const auto next = find(string, position, bottom, flags);
                    if (next.isNull())
                        break;
                    cursor.setPosition(next.anchor());
                    cursor.setPosition(next.position(), QTextCursor::KeepAnchor);
                }
                cursor.endEditBlock();
            }
        }
        action = FindReplaceBar::Refresh;
        mMarkedOccurrencesString.clear();
    }

    if (action == FindReplaceBar::FindTextChanged ||
        action == FindReplaceBar::Refresh) {

        auto cursor = find(string, textCursor().anchor(), bottom, 
            (flags & ~QTextDocument::FindBackward)); 
        if (inRange(cursor)) {
            setTextCursor(cursor);
            if (action != FindReplaceBar::Refresh)
                ensureCursorVisible();
        }
        else {
            cursor = textCursor();
            cursor.clearSelection();
            setTextCursor(cursor);          
        }
    }
    else if (action == FindReplaceBar::Find) {
        auto cursor = find(string, 
            (flags & QTextDocument::FindBackward ? 
                textCursor().position() - 1 : textCursor().anchor() + 1), 
            (flags & QTextDocument::FindBackward ? top : bottom), flags); 
        if (!inRange(cursor))
            cursor = find(string, 
                (flags & QTextDocument::FindBackward ? bottom : top),
                textCursor().position(), flags); 
        if (inRange(cursor)) {
            setTextCursor(cursor);
            ensureCursorVisible();
        }
    }

    markOccurrences(string, flags);
}

void SourceEditor::markOccurrences(QString text, QTextDocument::FindFlags flags)
{
    if (text.isEmpty() && mMarkedOccurrencesString.isEmpty())
        return;

    flags &= (~QTextDocument::FindBackward);
    const auto rect = QRect(horizontalScrollBar()->sliderPosition(),
        verticalScrollBar()->sliderPosition(), width(), height());

    if (mMarkedOccurrencesString == text && 
        mMarkedOccurrencesFindFlags == flags &&
        mMarkedOccurrencesRect == rect)
        return;

    mMarkedOccurrencesString = text;
    mMarkedOccurrencesFindFlags = flags;
    mMarkedOccurrencesRect = rect;

    mMarkedOccurrences.clear();
    if (!text.isEmpty()) {
        const auto findReplaceRange = updateFindReplaceRange();
        auto top = cursorForPosition(QPoint{ 0, 0 }).position();
        auto bottom = cursorForPosition(QPoint{ width(), height() }).position();
        top = std::max(top, findReplaceRange.anchor());
        bottom = std::min(bottom, findReplaceRange.position());
        for (auto it = find(text, top, bottom, flags); !it.isNull(); 
                it = find(text, it.position() + 1, bottom, flags))
            mMarkedOccurrences.append(it);
    }
    updateExtraSelections();
}

QTextCursor SourceEditor::updateFindReplaceRange()
{
    auto cursor = textCursor();
    if (cursor.selectedText().contains(QChar::ParagraphSeparator)) {
        if (cursor.position() < cursor.anchor()) {
            const auto tmp = cursor.anchor();
            cursor.setPosition(cursor.position());
            cursor.setPosition(tmp, QTextCursor::KeepAnchor);            
        }
        mFindReplaceRange = cursor;
        // invalidate current occurrences when selection changes
        mMarkedOccurrencesString.clear();
    }
    else if (!mFindReplaceRange.isNull()) {
        cursor = mFindReplaceRange;
    }
    else {
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    }
    return cursor;
}

void SourceEditor::clearMarkedOccurrences() 
{
    if (!mMarkedOccurrences.empty()) {
        mMarkedOccurrences.clear();
        mMarkedOccurrencesString.clear();
        updateExtraSelections();
    }
}

void SourceEditor::clearFindReplaceRange()
{
    if (!mFindReplaceRange.isNull()) {
        mFindReplaceRange = { };
        // clear occurrences when they were constrainted by range
        clearMarkedOccurrences();
        updateExtraSelections();
    }
}
