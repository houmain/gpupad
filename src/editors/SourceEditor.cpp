#include "SourceEditor.h"
#include "Singletons.h"
#include "FileCache.h"
#include "Settings.h"
#include "FindReplaceBar.h"
#include "FileDialog.h"
#include "GlslHighlighter.h"
#include "JsHighlighter.h"
#include "RectangularSelection.h"
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
#include <QSaveFile>

class SourceEditor::LineNumberArea : public QWidget
{
public:
    LineNumberArea(SourceEditor *editor)
        : QWidget(editor), mEditor(editor) { }

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

SourceEditor::SourceEditor(QString fileName, FindReplaceBar *findReplaceBar, QWidget *parent)
    : QPlainTextEdit(parent)
    , mFileName(fileName)
    , mFindReplaceBar(*findReplaceBar)
    , mLineNumberArea(new LineNumberArea(this))
{
    connect(this, &SourceEditor::textChanged,
        [this]() { Singletons::fileCache().invalidateEditorFile(mFileName); });
    connect(this, &SourceEditor::blockCountChanged,
        this, &SourceEditor::updateViewportMargins);
    connect(this, &SourceEditor::updateRequest,
        this, &SourceEditor::updateLineNumberArea);
    connect(this, &SourceEditor::cursorPositionChanged,
        this, &SourceEditor::updateExtraSelections);
    connect(this, &SourceEditor::textChanged,
        this, &SourceEditor::handleTextChanged);
    connect(&mFindReplaceBar, &FindReplaceBar::action,
        this, &SourceEditor::findReplaceAction);

    const auto &settings = Singletons::settings();
    setFont(settings.font());
    setTabSize(settings.tabSize());
    setLineWrap(settings.lineWrap());
    setAutoIndentation(settings.autoIndentation());
    setIndentWithSpaces(settings.indentWithSpaces());

    connect(&settings, &Settings::tabSizeChanged,
        this, &SourceEditor::setTabSize);
    connect(&settings, &Settings::fontChanged,
        this, &SourceEditor::setFont);
    connect(&settings, &Settings::lineWrapChanged,
        this, &SourceEditor::setLineWrap);
    connect(&settings, &Settings::autoIndentationChanged,
        this, &SourceEditor::setAutoIndentation);
    connect(&settings, &Settings::indentWithSpacesChanged,
        this, &SourceEditor::setIndentWithSpaces);
    connect(&settings, &Settings::syntaxHighlightingChanged,
        this, &SourceEditor::updateSyntaxHighlighting);
    connect(&settings, &Settings::darkThemeChanged,
        this, &SourceEditor::updateColors);
    connect(&settings, &Settings::darkThemeChanged,
        this, &SourceEditor::updateSyntaxHighlighting);

    updateViewportMargins();
    updateExtraSelections();
    updateColors(settings.darkTheme());
    setSourceTypeFromExtension();
    setPlainText(document()->toPlainText());
}

SourceEditor::~SourceEditor()
{
    disconnect(this, &SourceEditor::textChanged,
        this, &SourceEditor::handleTextChanged);
   disconnect(this, &SourceEditor::cursorPositionChanged,
        this, &SourceEditor::updateExtraSelections);

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

    return c;
}

void SourceEditor::setFileName(QString fileName)
{
    mFileName = fileName;
    emit fileNameChanged(mFileName);
}

bool SourceEditor::load(const QString &fileName, QString *source)
{
    if (!source)
        return false;

    if (FileDialog::isEmptyOrUntitled(fileName)) {
        *source = "";
        return true;
    }
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return false;

    QTextStream stream(&file);
    auto string = stream.readAll();
    if (!string.isSimpleText()) {
      stream.setCodec("Windows-1250");
      stream.seek(0);
      string = stream.readAll();
      const auto unprintable = std::find_if(string.constBegin(), string.constEnd(),
          [](QChar c) { return (!c.isPrint() && !c.isSpace()); });
      if (unprintable != string.constEnd())
        return false;
    }

    *source = string;
    return true;
}

bool SourceEditor::load()
{
    auto source = QString();
    if (!Singletons::fileCache().getSource(mFileName, &source))
        return false;

    setPlainText(source);
    return true;
}

bool SourceEditor::reload()
{
    auto source = QString();
    if (!load(mFileName, &source))
        return false;

    auto cursor = textCursor();
    auto position = cursor.position();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.insertText(source);
    cursor.setPosition(position);
    cursor.endEditBlock();
    setTextCursor(cursor);

    document()->setModified(false);
    return true;
}

bool SourceEditor::save()
{
    QSaveFile file(fileName());
    if (!file.open(QFile::WriteOnly | QFile::Text))
        return false;
    file.write(document()->toPlainText().toUtf8());
    if (!file.commit())
        return false;

    document()->setModified(false);
    setSourceTypeFromExtension();
    return true;
}

void SourceEditor::setSourceTypeFromExtension()
{
    const auto extension = QFileInfo(mFileName).suffix();

    if (extension == "glsl" && mSourceType == SourceType::PlainText)
        setSourceType(SourceType::FragmentShader);
    else if (extension == "vs" || extension == "vert")
        setSourceType(SourceType::VertexShader);
    else if (extension == "fs" || extension == "frag")
        setSourceType(SourceType::FragmentShader);
    else if (extension == "gs" || extension == "geom")
        setSourceType(SourceType::GeometryShader);
    else if (extension == "tesc")
        setSourceType(SourceType::TesselationControl);
    else if (extension == "tese")
        setSourceType(SourceType::TesselationEvaluation);
    else if (extension == "comp")
        setSourceType(SourceType::ComputeShader);
    else if (extension == "js")
        setSourceType(SourceType::JavaScript);
}

void SourceEditor::setSourceType(SourceType sourceType)
{
    if (mSourceType != sourceType) {
        mSourceType = sourceType;
        updateSyntaxHighlighting();
    }
}

void SourceEditor::updateColors(bool darkTheme)
{
    mCurrentLineFormat.setProperty(QTextFormat::FullWidthSelection, true);
    mCurrentLineFormat.setBackground(palette().base().color().lighter(darkTheme ? 110 : 95));

    mOccurrencesFormat.setForeground(palette().text().color().lighter(darkTheme ? 120 : 90));
    mOccurrencesFormat.setBackground(palette().base().color().lighter(darkTheme ? 120 : 90));

    mMultiSelectionFormat.setForeground(palette().highlightedText());
    mMultiSelectionFormat.setBackground(palette().highlight());

    auto window = palette().window().color();
    mLineNumberColor = window.darker(window.value() < 128 ? 50 : 150);

    updateExtraSelections();
}

void SourceEditor::updateSyntaxHighlighting()
{
    const auto disabled =
        (document()->characterCount() > (1 << 20) ||
         !Singletons::settings().syntaxHighlighting() ||
         mSourceType == SourceType::PlainText);

    if (disabled) {
        delete mHighlighter;
        mHighlighter = nullptr;
        mCompleter = nullptr;
        return;
    }

    const auto &settings = Singletons::settings();

    if (mSourceType == SourceType::JavaScript) {
        auto highlighter = new JsHighlighter(settings.darkTheme(), this);
        delete mHighlighter;
        mHighlighter = highlighter;
        mCompleter = highlighter->completer();
    }
    else {
        auto highlighter = new GlslHighlighter(settings.darkTheme(), this);
        delete mHighlighter;
        mHighlighter = highlighter;
        mCompleter = highlighter->completer();
    }

    mHighlighter->setDocument(document());
    mCompleter->setWidget(this);
    mCompleter->setCompletionMode(QCompleter::PopupCompletion);
    mCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    connect(mCompleter, qOverload<const QString &>(&QCompleter::activated),
        this, &SourceEditor::insertCompletion);
}

void SourceEditor::findReplace()
{
    mFindReplaceBar.focus(this, textUnderCursor());
}

void SourceEditor::setFont(const QFont &font)
{
    QPlainTextEdit::setFont(font);
    setTabSize(mTabSize);
}

void SourceEditor::setTabSize(int tabSize)
{
    mTabSize = tabSize;
    setTabStopDistance(fontMetrics().horizontalAdvance(
        QString(tabSize, QChar::Space)));
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

int SourceEditor::lineNumberAreaWidth()
{
    auto digits = 1;
    auto max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return fontMetrics().horizontalAdvance(QString(digits, '9'));
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

void SourceEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    if (!mMultiSelections.empty()) {
        QPainter painter(viewport());
        for (const auto &selection : mMultiSelections)
            if (selection.anchor() == selection.position()) {
            const auto rect = cursorRect(selection);
            painter.fillRect(rect.x(), rect.y(), 1, rect.height(),
                QPalette().text());
        }
    }
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
    auto spaces = QString(line).replace(QRegularExpression("^(\\s*).*"), "\\1");
    if (line.endsWith('{'))
        spaces += tab();

    cursor.insertText(line + newParagraph + spaces);
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
    else {
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
    if (mCompleter && mCompleter->popup()->isVisible()) {
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
    const auto multiSelectionModifierHold = (event->modifiers() & Qt::AltModifier) &&
                                            (event->modifiers() & Qt::ShiftModifier);

    if (!mMultiSelections.empty() || multiSelectionModifierHold) {
        if (!ctrlHold && updateMultiSelection(event, multiSelectionModifierHold))
            return;
        endMultiSelection();
    }

    if (ctrlHold && event->key() == Qt::Key_F) {
        findReplace();
    }
    else if (mAutoIndentation && event->key() == Qt::Key_Escape) {
        mFindReplaceBar.cancel();
    }
    else if (mAutoIndentation && (event->key() == Qt::Key_Return ||
                                  event->key() == Qt::Key_Enter)) {
        autoIndentNewLine();
        removeTrailingSpace();
    }
    else if (mAutoIndentation && event->key() == Qt::Key_BraceRight) {
        autoDeindentBrace();
    }
    else if (mAutoIndentation && event->key() == Qt::Key_Home) {
        toggleHomePosition(event->modifiers() & Qt::ShiftModifier);
    }
    else if (event->key() == Qt::Key_Tab) {
        indentSelection();
    }
    else if (event->key() == Qt::Key_Backtab) {
        indentSelection(true);
    }
    else {
        QPlainTextEdit::keyPressEvent(event);

        const auto hitCtrlOrShift = event->modifiers() &
            (Qt::ControlModifier | Qt::ShiftModifier);
        if (hitCtrlOrShift && event->text().isEmpty())
            return;

        const auto prefix = textUnderCursor();
        const auto textEntered = !event->text().trimmed().isEmpty();
        const auto hitCtrlSpace = ((event->modifiers() & Qt::ControlModifier) &&
            event->key() == Qt::Key_Space);

        const auto show = ((textEntered && prefix.size() >= 3) || hitCtrlSpace);
        updateCompleterPopup(prefix, show);
    }
}

void SourceEditor::beginMultiSelection()
{
    mMultiEditCursor = { };
    setCursorWidth(0);
    if (mMultiSelections.empty())
        mMultiSelections.append(textCursor());
}

void SourceEditor::endMultiSelection()
{
    if (!mMultiSelections.isEmpty()) {
        auto cursor = textCursor();
        auto position = cursor.position();
        cursor.setPosition(mMultiSelections.front().anchor());
        cursor.setPosition(position, QTextCursor::KeepAnchor);
        setTextCursor(cursor);

        mMultiSelections.clear();
        updateExtraSelections();
        setCursorWidth(1);
    }
}

bool SourceEditor::updateMultiSelection(QKeyEvent *event, bool multiSelectionModifierHold)
{
    const auto withEachSelection = [&](const auto& function) {
        if (mMultiEditCursor.isNull()) {
            mMultiEditCursor = textCursor();
            mMultiEditCursor.beginEditBlock();
        }
        else {
            mMultiEditCursor.joinPreviousEditBlock();
        }
        for (auto &selection : mMultiSelections) {
            mMultiEditCursor.setPosition(selection.anchor());
            mMultiEditCursor.setPosition(selection.position(), QTextCursor::KeepAnchor);
            function(mMultiEditCursor);
            selection = mMultiEditCursor;
        }
        mMultiEditCursor.endEditBlock();
    };

    if (!multiSelectionModifierHold) {
        switch (event->key()) {
            case Qt::Key_Escape:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return false;

            case Qt::Key_Backspace:
                withEachSelection([&](QTextCursor& selection) {
                    if (!selection.atBlockStart())
                        selection.deletePreviousChar();
                });
                return true;

            case Qt::Key_Delete:
                withEachSelection([&](QTextCursor& selection) {
                    if (!selection.atBlockEnd())
                        selection.deleteChar();
                });
                return true;

            case Qt::Key_Tab:
                if (!(event->modifiers() & Qt::ShiftModifier))
                    withEachSelection([&](QTextCursor& selection) {
                        selection.insertText(tab());
                    });
                return true;

            default:
                if (!event->text().isEmpty())
                    withEachSelection([&](QTextCursor& selection) {
                        selection.insertText(event->text());
                    });
                return true;
        }
    }

    beginMultiSelection();
    auto selection = RectangularSelection(&mMultiSelections);
    switch (event->key()) {
        case Qt::Key_Up: selection.moveUp(); break;
        case Qt::Key_Down: selection.moveDown(); break;
        case Qt::Key_Left: selection.moveLeft(); break;
        case Qt::Key_Right: selection.moveRight(); break;
    }
    setTextCursor(mMultiSelections.last());
    updateExtraSelections();
    return true;
}

void SourceEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        auto font = this->font();
        font.setPointSize(font.pointSize() + (event->delta() > 0 ? 1 : -1));
        Singletons::settings().setFont(font);
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void SourceEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPlainTextEdit::mouseDoubleClickEvent(event);
    markOccurrences(textUnderCursor(true));
}

void SourceEditor::mousePressEvent(QMouseEvent *event)
{
    endMultiSelection();
    QPlainTextEdit::mousePressEvent(event);
}

void SourceEditor::mouseMoveEvent(QMouseEvent *event)
{
    QPlainTextEdit::mouseMoveEvent(event);
    if (event->modifiers() & Qt::AltModifier) {
        beginMultiSelection();
        mMultiSelections.last() = textCursor();
        RectangularSelection selection(&mMultiSelections);
        selection.create();
        setTextCursor(mMultiSelections.last());
        updateExtraSelections();
    }
    else {
        endMultiSelection();
    }
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

void SourceEditor::handleTextChanged()
{
    if (!mMarkedOccurrences.empty()) {
        mMarkedOccurrences.clear();
        updateExtraSelections();
    }
}

void SourceEditor::updateExtraSelections()
{
    auto selections = QList<QTextEdit::ExtraSelection>();
    auto cursor = textCursor();
    cursor.clearSelection();
    selections.append({ cursor, mCurrentLineFormat });

    for (const auto &occurrence : mMarkedOccurrences)
        selections.append({ occurrence, mOccurrencesFormat });

    for (const auto &selection : mMultiSelections)
        selections.append({ selection, mMultiSelectionFormat });

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

QString SourceEditor::textUnderCursor(bool identifierOnly) const
{
    auto cursor = textCursor();
    if (cursor.selectedText().isEmpty()) {
        cursor.movePosition(QTextCursor::Left);
        cursor.select(QTextCursor::WordUnderCursor);
    }
    auto text = cursor.selectedText();
    if (identifierOnly)
        text = text.replace(QRegularExpression("[^a-zA-Z0-9_]"), "");
    return text;
}

void SourceEditor::updateCompleterPopup(const QString &prefix, bool show)
{
    if (!mCompleter)
        return;

    if (show) {
        mCompleter->setCompletionPrefix(prefix);
        const auto alreadyComplete =
          (mCompleter->currentCompletion() == prefix &&
           mCompleter->completionCount() == 1);

        if (!alreadyComplete) {
            mCompleter->popup()->setCurrentIndex(
                mCompleter->completionModel()->index(0, 0));
            auto rect = cursorRect();
            rect.adjust(0, 1, 0, 1);
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

void SourceEditor::findReplaceAction(FindReplaceBar::Action action,
    QString find, QString replace, QTextDocument::FindFlags flags)
{
    if (mFindReplaceBar.target() != this)
        return;

    markOccurrences(find, flags & (~QTextDocument::FindBackward));

    if (find.isEmpty())
        return;

    if (action == FindReplaceBar::ReplaceAll) {
        auto cursor = textCursor();
        cursor.beginEditBlock();
        for (const auto &occurrence : qAsConst(mMarkedOccurrences)) {
            cursor.setPosition(occurrence.anchor());
            cursor.setPosition(occurrence.position(), QTextCursor::KeepAnchor);
            cursor.insertText(replace);
        }
        cursor.endEditBlock();
        return;
    }

    if (action == FindReplaceBar::Replace) {
        auto cursor = textCursor();
        if (!cursor.selectedText().compare(find, Qt::CaseInsensitive))
            cursor.insertText(replace);
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
