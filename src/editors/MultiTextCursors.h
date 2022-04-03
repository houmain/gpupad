#pragma once

#include <QObject>
#include <QList>
#include <QTextCursor>

class QKeyEvent;
class QMouseEvent;

class MultiTextCursors : public QObject
{
    Q_OBJECT
public:
    explicit MultiTextCursors(QObject *parent = nullptr);

    const QList<QTextCursor> &cursors() const { return mCursors; }

    bool handleKeyPressEvent(QKeyEvent *event, QTextCursor cursor);
    bool handleMouseDoubleClickEvent(QMouseEvent *event, QTextCursor cursor);
    void handleBeforeMousePressEvent(QMouseEvent *event, QTextCursor cursor);
    void handleMousePressedEvent(QMouseEvent *event, QTextCursor cursor, QTextCursor prevCursor);
    void handleMouseMoveEvent(QMouseEvent *event, QTextCursor cursor);
    void clear();
    void clear(QTextCursor cursor);
    void setTab(QString tab) { mTab = tab; }
    bool cut();
    bool copy();
    bool paste();

Q_SIGNALS:
    void cursorChanged(const QTextCursor &);
    void cursorsChanged();

private:
    void createRectangularSelection(QTextCursor cursor);
    int getVerticallyMoved(QTextCursor cursor, int inBlockNumber);
    void addLine(QTextCursor::MoveOperation operation);
    QTextCursor getMaxColumnCursor() const;
    void rectangularSelectUp();
    void rectangularSelectDown();
    void rectangularSelectLeft();
    void rectangularSelectRight();
    void rectangularSelectPageUp();
    void rectangularSelectPageDown();
    bool isBottomUp() const;
    void emitChanged();
    void mergeCursors();
    void moveEachSelection(QTextCursor::MoveOperation op, QTextCursor::MoveMode mode);
    void reverseOnBottomUpSelection(QStringList &lines);

    QList<QTextCursor> mPrevCursors;
    QList<QTextCursor> mCursors;
    bool mCursorRemoved{ };
    bool mHasRectangularSelection{ };
    int mPageSize{ 20 };
    QString mTab{ "  " };
};
