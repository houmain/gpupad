#pragma once

class QAction;

struct EditActions
{
    QAction *windowFileName;
    QAction *undo;
    QAction *redo;
    QAction *cut;
    QAction *copy;
    QAction *paste;
    QAction *delete_;
    QAction *selectAll;
    QAction *rename;
    QAction *findReplace;
};
