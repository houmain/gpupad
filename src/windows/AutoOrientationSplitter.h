#ifndef AUTOORIENTATIONSPLITTER_H
#define AUTOORIENTATIONSPLITTER_H

#include <QSplitter>

class AutoOrientationSplitter final : public QSplitter
{
    Q_OBJECT
public:
    AutoOrientationSplitter(QWidget *parent) : QSplitter(parent)
    {
        setFrameShape(QFrame::StyledPanel);
        setChildrenCollapsible(false);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        setOrientation(width() > height() ?
            Qt::Horizontal : Qt::Vertical);

        const auto vertical = (orientation() == Qt::Vertical);
        setStretchFactor(vertical ? 1 : 0, 0);
        setStretchFactor(vertical ? 0 : 1, 100);

        QSplitter::resizeEvent(event);
    }
};

#endif // AUTOORIENTATIONSPLITTER_H
