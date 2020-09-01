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
    if (mDecimal) {
        stepBy(event->modifiers() & Qt::ShiftModifier ? steps / 10.0 :
               event->modifiers() & Qt::ControlModifier ? steps * 10.0 : steps);
    }
    else {
        stepBy(event->modifiers() & Qt::ControlModifier ? steps * 10 : steps);
    }
    event->accept();
}

void ExpressionLineEdit::stepBy(int steps)
{
    auto ok = false;
    auto value = text().toInt(&ok);
    if (ok) {
        value = qMax(value + steps, 0);
        setText(QString::number(value));
        selectAll();
    }
}

void ExpressionLineEdit::stepBy(double steps)
{
    const auto singleStep = 0.1;
    auto ok = false;
    auto value = text().toDouble(&ok);
    if (ok) {
        value += steps * singleStep;
        setText(QString::number(value, 'f',
            value == qRound(value) ? 0 : 2));
        selectAll();
    }
}
