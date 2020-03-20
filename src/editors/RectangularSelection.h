#ifndef RECTANGULARSELECTION_H
#define RECTANGULARSELECTION_H

#include <QList>
#include <QTextCursor>

class RectangularSelection
{
public:
    explicit RectangularSelection(QList<QTextCursor>* cursors)
        : cursors(*cursors)
    {
        Q_ASSERT(!cursors->isEmpty());
        if (cursors->size() == 1)
            create();
    }

    void create()
    {
        auto& cursor = cursors.front();
        if (cursors.size() > 1) {
            // reset to sole cursor with anchor of first and position of last
            cursor.setPosition(cursor.anchor());
            cursor.setPosition(cursors.last().position(), QTextCursor::KeepAnchor);
            while (cursors.size() > 1)
                cursors.removeLast();
        }

        auto anchor = cursor;
        anchor.setPosition(cursor.anchor());

        if (anchor.columnNumber() < cursor.columnNumber()) {
            // create a cursor for each line between position and anchor
            const auto position = cursor.position();
            const auto lines = anchor.blockNumber() - cursor.blockNumber();
            cursor.setPosition(getVerticallyMoved(anchor, cursor.blockNumber()));
            cursor.setPosition(position, QTextCursor::KeepAnchor);
            for (auto i = 0; i < std::abs(lines); ++i)
                addLine(lines > 0 ? QTextCursor::Down : QTextCursor::Up);
            std::reverse(cursors.begin(), cursors.end());
        }
        else {
            // create a cursor for each line between anchor and position
            const auto lines = cursor.blockNumber() - anchor.blockNumber();
            anchor.setPosition(getVerticallyMoved(cursor, anchor.blockNumber()), QTextCursor::KeepAnchor);
            cursor = anchor;
            for (auto i = 0; i < std::abs(lines); ++i)
                addLine(lines > 0 ? QTextCursor::Down : QTextCursor::Up);
        }
    }

    void moveUp()
    {
        if (!isBottomUp() && cursors.size() > 1) {
            cursors.removeLast();
        }
        else {
            addLine(QTextCursor::Up);
        }
    }

    void moveDown()
    {
        if (!isBottomUp()) {
            addLine(QTextCursor::Down);
        }
        else {
            cursors.removeLast();
        }
    }

    void moveLeft()
    {
        auto position = getMaxColumnCursor();
        if (!position.atBlockStart()) {
            position.setPosition(position.position());
            position.movePosition(QTextCursor::Left);
            for (auto &cursor : cursors)
                cursor.setPosition(
                    getVerticallyMoved(position, cursor.blockNumber()),
                    QTextCursor::KeepAnchor);
        }
    }

    void moveRight()
    {
        for (auto position : cursors)
            if (!position.atBlockEnd()) {
                position.setPosition(position.position());
                position.movePosition(QTextCursor::Right);
                for (auto &cursor : cursors)
                    cursor.setPosition(
                        getVerticallyMoved(position, cursor.blockNumber()),
                        QTextCursor::KeepAnchor);
                break;
            }
    }

private:
    QList<QTextCursor>& cursors;

    bool isBottomUp() const
    {
        return (cursors.last().blockNumber() < cursors.first().blockNumber());
    }

    QTextCursor getMaxColumnCursor() const
    {
        return *std::max_element(cursors.begin(), cursors.end(),
            [&](const auto &a, const auto &b) {
                return (a.columnNumber() < b.columnNumber());
            });
    }

    int getVerticallyMoved(QTextCursor cursor, int inBlockNumber) const
    {
        const auto dir = inBlockNumber - cursor.blockNumber();
        cursor.movePosition(
            (dir > 0 ? QTextCursor::Down : QTextCursor::Up),
            QTextCursor::MoveAnchor,
            std::abs(dir));
        return cursor.position();
    }

    void addLine(QTextCursor::MoveOperation operation)
    {
        auto cursor = cursors.first();
        cursor.setPosition(cursor.anchor());
        if (cursor.movePosition(operation, QTextCursor::MoveAnchor, cursors.size())) {
            cursor.setPosition(
                getVerticallyMoved(getMaxColumnCursor(), cursor.blockNumber()),
                QTextCursor::KeepAnchor);
            cursors.append(cursor);
        }
    }
};

#endif // RECTANGULARSELECTION_H
