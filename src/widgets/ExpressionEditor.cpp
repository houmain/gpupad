#include "ExpressionEditor.h"
#include <QWheelEvent>

extern QString simpleDoubleString(double value);

ExpressionEditor::ExpressionEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    setTabChangesFocus(true);
}

void ExpressionEditor::wheelEvent(QWheelEvent *event)
{
    mWheelDeltaRemainder += event->angleDelta().y();
    const int steps = mWheelDeltaRemainder / 120;
    mWheelDeltaRemainder -= steps * 120;
    stepBy(event->modifiers() & Qt::ShiftModifier      ? steps / 10.0
            : event->modifiers() & Qt::ControlModifier ? steps * 10.0
                                                       : steps);
    event->accept();
}

void ExpressionEditor::stepBy(double steps)
{
    const auto singleStep = 0.1;
    auto ok = false;
    auto value = toPlainText().toDouble(&ok);
    if (ok) {
        value += steps * singleStep;
        setText(simpleDoubleString(value));
        selectAll();
    }
}

void ExpressionEditor::setText(const QString &text)
{
    if (text != toPlainText())
        setPlainText(text);
}

QString ExpressionEditor::text() const
{
    return toPlainText();
}
