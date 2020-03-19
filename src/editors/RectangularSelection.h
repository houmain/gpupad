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
        convertToRectangle();
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

    void convertToRectangle()
    {
        if (cursors.size() == 1) {
            auto& cursor = cursors.front();
            auto anchor = cursor;
            anchor.setPosition(cursor.anchor());
            const auto lines = cursor.blockNumber() - anchor.blockNumber();
            anchor.setPosition(getVerticallyMoved(cursor, anchor.blockNumber()), QTextCursor::KeepAnchor);
            cursor = anchor;
            for (auto i = 0; i < std::abs(lines); ++i)
                addLine(lines > 0 ? QTextCursor::Down : QTextCursor::Up);
        }
    }

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
