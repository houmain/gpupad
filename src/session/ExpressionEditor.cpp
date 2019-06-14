#include "ExpressionEditor.h"
#include <QWheelEvent>

ExpressionEditor::ExpressionEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    document()->setDocumentMargin(0);
    setViewportMargins(3, 0, 0, 0);
}

void ExpressionEditor::wheelEvent(QWheelEvent *event)
{
    mWheelDeltaRemainder += event->angleDelta().y();
    const int steps = mWheelDeltaRemainder / 120;
    mWheelDeltaRemainder -= steps * 120;
    stepBy(event->modifiers() & Qt::ShiftModifier ? steps / 10.0 :
           event->modifiers() & Qt::ControlModifier ? steps * 10.0 : steps);
    selectAll();
    event->accept();
}

void ExpressionEditor::stepBy(double steps)
{
    const auto singleStep = 0.1;
    auto ok = false;
    auto value = toPlainText().toDouble(&ok);
    if (ok) {
        value += steps * singleStep;
        setPlainText(QString::number(value, 'f',
            value == qRound(value) ? 0 : 2));
    }
}
