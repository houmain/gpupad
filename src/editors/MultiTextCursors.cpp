
#include "MultiTextCursors.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>

namespace {
    bool inRange(int pos, int min, int max)
    {
        if (min == max)
            return pos == min;
        if (min < max)
            return (pos > min && pos < max);
        return (pos > max && pos < min);
    }

    bool mergeCursors(const QTextCursor &a, const QTextCursor &b)
    {
        const auto aa = a.anchor();
        const auto ap = a.position();
        const auto ba = b.anchor();
        const auto bp = b.position();
        return (inRange(aa, ba, bp) || inRange(ap, ba, bp) ||
                inRange(ba, aa, ap) || inRange(bp, aa, ap));
    }

    template<typename F>
    void editEachSelection(QList<QTextCursor>& cursors, F&& function)
    {
        if (cursors.isEmpty())
            return;

        auto editCursor = cursors.first();
        editCursor.beginEditBlock();

        for (auto &selection : cursors) {
            editCursor.setPosition(selection.anchor());
            editCursor.setPosition(selection.position(), QTextCursor::KeepAnchor);
            function(editCursor);
            selection = editCursor;
        }
        editCursor.endEditBlock();
    }
} // namespace

MultiTextCursors::MultiTextCursors(QObject *parent)
    : QObject(parent)
{
}

bool MultiTextCursors::handleKeyPressEvent(QKeyEvent *event, QTextCursor cursor)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->modifiers() & Qt::ShiftModifier) {
            switch (event->key()) {
                case Qt::Key_Up:
                case Qt::Key_Down:
                case Qt::Key_Left:
                case Qt::Key_Right:
                case Qt::Key_PageUp:
                case Qt::Key_PageDown:
                    if (!mHasRectangularSelection)
                        createRectangularSelection(cursor);
                    break;
            }
            switch (event->key()) {
                case Qt::Key_Up: rectangularSelectUp(); break;
                case Qt::Key_Down: rectangularSelectDown(); break;
                case Qt::Key_Left: rectangularSelectLeft(); break;
                case Qt::Key_Right: rectangularSelectRight(); break;
                case Qt::Key_PageUp: rectangularSelectPageUp(); break;
                case Qt::Key_PageDown: rectangularSelectPageDown(); break;
            }
        }
        emitChanged();
        return true;
    }

    mHasRectangularSelection = false;

    const auto reverseOnBottomUpSelection = [&](QStringList &lines) {
        if (mCursors.size() > 1 &&
            mCursors.front().position() > mCursors.back().position())
            std::reverse(lines.begin(), lines.end());
    };

    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
            case Qt::Key_X:
            case Qt::Key_C: {
                auto lines = QStringList();
                editEachSelection(mCursors, [&](QTextCursor& selection) {
                    lines.append(selection.selectedText());
                });
                reverseOnBottomUpSelection(lines);
                QApplication::clipboard()->setText(lines.join("\n"));

                if (event->key() ==  Qt::Key_X)
                    editEachSelection(mCursors, [&](QTextCursor& selection) {
                        selection.insertText("");
                    });
                emitChanged();
                return true;
            }

            case Qt::Key_V: {
                auto lines = QApplication::clipboard()->text().remove("\r").split("\n");
                if (lines.size() == 1) {
                    editEachSelection(mCursors, [&](QTextCursor& selection) {
                        selection.insertText(lines[0]);
                    });
                }
                else {
                    if (lines.size() > mCursors.size())
                        lines = lines.mid(0, mCursors.size());
                    while (lines.size() < mCursors.size())
                        lines.append("");
                    reverseOnBottomUpSelection(lines);

                    auto i = 0;
                    editEachSelection(mCursors, [&](QTextCursor& selection) {
                        selection.insertText(lines[i++]);
                    });
                }
                emitChanged();
                return true;
            }

            default:
                if (event->key() >= Qt::Key_A && event->key() <= Qt::Key_Z) {
                    clear(cursor);
                    return false;
                }
        }
    }

    if (mCursors.isEmpty())
        return false;

    const auto mode = (event->modifiers() & Qt::ShiftModifier ?
        QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
    const auto ctrlDown = (event->modifiers() & Qt::ControlModifier);
    switch (event->key()) {
        case Qt::Key_Escape:
            clear(cursor);
            return false;

        case Qt::Key_Up: moveEachSelection(QTextCursor::Up, mode); break;
        case Qt::Key_Down: moveEachSelection(QTextCursor::Down, mode); break;
        case Qt::Key_Left: moveEachSelection(
            ctrlDown ? QTextCursor::PreviousWord : QTextCursor::Left, mode); break;
        case Qt::Key_Right: moveEachSelection(
            ctrlDown ? QTextCursor::NextWord : QTextCursor::Right, mode); break;
        case Qt::Key_Home: moveEachSelection(QTextCursor::StartOfBlock, mode); break;
        case Qt::Key_End: moveEachSelection(QTextCursor::EndOfBlock, mode); break;

        case Qt::Key_Backspace:
            editEachSelection(mCursors, [&](QTextCursor& s) {
                if (mHasRectangularSelection && (s.position() != s.anchor() || s.atBlockStart()))
                    s.removeSelectedText();
                else
                    s.deletePreviousChar();
            });
            return true;

        case Qt::Key_Delete:
            editEachSelection(mCursors, [&](QTextCursor& s) {
                if (mHasRectangularSelection && (s.position() != s.anchor() || s.atBlockEnd()))
                    s.removeSelectedText();
                else
                    s.deleteChar();
            });
            return true;

        case Qt::Key_Tab:
            if (!(event->modifiers() & Qt::ShiftModifier))
                editEachSelection(mCursors, [&](QTextCursor& selection) {
                    selection.insertText(mTab);
                });
            return true;

        default:
            if (const auto text = event->text(); !text.isEmpty())
                editEachSelection(mCursors, [&](QTextCursor& s) { s.insertText(text); });
            break;
    }
    emitChanged();
    return true;
}

bool MultiTextCursors::handleMouseDoubleClickEvent(QMouseEvent *event, QTextCursor cursor)
{
    if (event->modifiers() & Qt::AltModifier) {
        handleBeforeMousePressEvent(event, cursor);
        if (mCursorRemoved) {
            cursor.select(QTextCursor::WordUnderCursor);
            mCursors.append(cursor);
        }
        emitChanged();
        return true;
    }
    return false;
}

void MultiTextCursors::handleBeforeMousePressEvent(QMouseEvent *event, QTextCursor cursor)
{
    if (event->modifiers() & Qt::AltModifier) {
        const auto it = std::find_if(mCursors.begin(), mCursors.end(),
            [position = cursor.position()](const QTextCursor& cursor) {
                return (cursor.position() == position) ||
                       (cursor.anchor() < position && cursor.position() >= position) ||
                       (cursor.position() <= position && cursor.anchor() > position);
            });

        mCursorRemoved = (it != mCursors.end());
        if (it != mCursors.end())
            mCursors.erase(it);

        emitChanged();
    }
}

void MultiTextCursors::handleMousePressedEvent(
    QMouseEvent *event, QTextCursor cursor, QTextCursor prevCursor)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->modifiers() & Qt::ShiftModifier) {
            mCursors = { (mCursors.isEmpty() ? prevCursor : mCursors.last()) };
            return createRectangularSelection(cursor);
        }

        if (mCursors.isEmpty())
            mCursors.append(prevCursor);

        if (!mCursorRemoved) {
            mCursors.append(cursor);
            mHasRectangularSelection = false;
        }
        emitChanged();
    }
    else {
        clear(cursor);
    }
}

void MultiTextCursors::handleMouseMoveEvent(QMouseEvent *event, QTextCursor cursor)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->modifiers() & Qt::ShiftModifier)
            return createRectangularSelection(cursor);

        if (std::exchange(mCursorRemoved, false))
            mCursors.append(cursor);

        if (!mCursors.isEmpty() && mCursors.last() != cursor) {
            mCursors.last() = cursor;
            mHasRectangularSelection = false;
            emitChanged();
        }
    }
}

void MultiTextCursors::clear()
{
    if (!mCursors.isEmpty()) {
        auto last = mCursors.last();
        last.setPosition(last.position());
        clear(last);
    }
}

void MultiTextCursors::clear(QTextCursor cursor)
{
    if (!mCursors.isEmpty()) {
        mCursors.clear();
        Q_EMIT cursorChanged(cursor);
        Q_EMIT cursorsChanged();
    }
    mHasRectangularSelection = false;
}

int MultiTextCursors::getVerticallyMoved(QTextCursor cursor, int inBlockNumber)
{
    const auto dir = inBlockNumber - cursor.blockNumber();
    cursor.movePosition(
        (dir > 0 ? QTextCursor::Down : QTextCursor::Up),
        QTextCursor::MoveAnchor,
        std::abs(dir));
    return cursor.position();
}

void MultiTextCursors::addLine(QTextCursor::MoveOperation operation)
{
    auto cursor = mCursors.first();
    cursor.setPosition(cursor.anchor());
    if (cursor.movePosition(operation, QTextCursor::MoveAnchor, mCursors.size())) {
        cursor.setPosition(
            getVerticallyMoved(getMaxColumnCursor(), cursor.blockNumber()),
            QTextCursor::KeepAnchor);
        mCursors.append(cursor);
    }
}

void MultiTextCursors::createRectangularSelection(QTextCursor cursor)
{
    auto first = cursor;
    if (!mCursors.isEmpty())
        first.setPosition(mCursors.first().anchor());
    mCursors = { cursor };
    auto& last = mCursors.last();

    if (first.columnNumber() < last.columnNumber()) {
        // create a cursor for each line between position and first
        const auto position = last.position();
        const auto lines = first.blockNumber() - last.blockNumber();
        last.setPosition(getVerticallyMoved(first, last.blockNumber()));
        last.setPosition(position, QTextCursor::KeepAnchor);
        for (auto i = 0; i < std::abs(lines); ++i)
            addLine(lines > 0 ? QTextCursor::Down : QTextCursor::Up);
        std::reverse(mCursors.begin(), mCursors.end());
    }
    else {
        // create a cursor for each line between first and position
        const auto lines = last.blockNumber() - first.blockNumber();
        first.setPosition(
            getVerticallyMoved(last, first.blockNumber()),
            QTextCursor::KeepAnchor);
        last = first;
        for (auto i = 0; i < std::abs(lines); ++i)
            addLine(lines > 0 ? QTextCursor::Down : QTextCursor::Up);
    }
    mHasRectangularSelection = true;
    emitChanged();
}

void MultiTextCursors::emitChanged()
{
    mergeCursors();

    if (mCursors != mPrevCursors) {
        if (!mCursors.isEmpty())
            Q_EMIT cursorChanged(mCursors.last());
        Q_EMIT cursorsChanged();
        mPrevCursors = mCursors;
    }
}

void MultiTextCursors::mergeCursors()
{
    if (mCursors.size() < 2)
        return;

    auto last = mCursors.last();

    std::sort(mCursors.begin(), mCursors.end());
    mCursors.erase(std::unique(mCursors.begin(), mCursors.end(), ::mergeCursors),
        mCursors.end());

    if (auto i = mCursors.indexOf(last); i >= 0) {
        last = mCursors[i];
        mCursors.removeAt(i);
        mCursors.append(last);
    }
}

QTextCursor MultiTextCursors::getMaxColumnCursor() const
{
    return *std::max_element(mCursors.begin(), mCursors.end(),
        [&](const auto &a, const auto &b) {
            return (a.columnNumber() < b.columnNumber());
        });
}

void MultiTextCursors::rectangularSelectUp()
{
    if (!isBottomUp() && mCursors.size() > 1) {
        mCursors.removeLast();
    }
    else {
        addLine(QTextCursor::Up);
    }
}

void MultiTextCursors::rectangularSelectDown()
{
    if (!isBottomUp()) {
        addLine(QTextCursor::Down);
    }
    else {
        mCursors.removeLast();
    }
}

void MultiTextCursors::rectangularSelectLeft()
{
    auto position = getMaxColumnCursor();
    if (!position.atBlockStart()) {
        position.setPosition(position.position());
        position.movePosition(QTextCursor::Left);
        for (auto &cursor : mCursors)
            cursor.setPosition(
                getVerticallyMoved(position, cursor.blockNumber()),
                QTextCursor::KeepAnchor);
    }
}

void MultiTextCursors::rectangularSelectRight()
{
    for (auto position : qAsConst(mCursors))
        if (!position.atBlockEnd()) {
            position.setPosition(position.position());
            position.movePosition(QTextCursor::Right);
            for (auto &cursor : mCursors)
                cursor.setPosition(
                    getVerticallyMoved(position, cursor.blockNumber()),
                    QTextCursor::KeepAnchor);
            break;
        }
}

void MultiTextCursors::rectangularSelectPageUp()
{
    for (auto i = 0; i < mPageSize; ++i)
        rectangularSelectUp();
}

void MultiTextCursors::rectangularSelectPageDown()
{
    for (auto i = 0; i < mPageSize; ++i)
        rectangularSelectDown();
}

bool MultiTextCursors::isBottomUp() const
{
    return (mCursors.last().blockNumber() < mCursors.first().blockNumber());
}

void MultiTextCursors::moveEachSelection(
    QTextCursor::MoveOperation op, QTextCursor::MoveMode mode)
{
    for (auto &cursor : mCursors)
        cursor.movePosition(op, mode);
}

