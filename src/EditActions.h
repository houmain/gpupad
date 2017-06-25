#ifndef EDITACTIONS_H
#define EDITACTIONS_H

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

#endif // EDITACTIONS_H
