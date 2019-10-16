#include "ExpressionLineEdit.h"
#include <QWheelEvent>

ExpressionLineEdit::ExpressionLineEdit(QWidget *parent) : QLineEdit(parent)
{
}

void ExpressionLineEdit::wheelEvent(QWheelEvent *event)
{
    mWheelDeltaRemainder += event->angleDelta().y();
    const auto steps = mWheelDeltaRemainder / 120;
    mWheelDeltaRemainder -= steps * 120;
    stepBy(event->modifiers() & Qt::ControlModifier ? steps * 10 : steps);
    event->accept();
}

void ExpressionLineEdit::stepBy(int steps)
{
    auto ok = false;
    auto value = text().toInt(&ok);
    if (ok) {
        value += steps;
        value = qMax(value, 0);
        setText(QString::number(value));
    }
}
