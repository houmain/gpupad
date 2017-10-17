#ifndef EXPRESSIONEDITOR_H
#define EXPRESSIONEDITOR_H

#include <QPlainTextEdit>

class ExpressionEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit ExpressionEditor(QWidget *parent = nullptr);

    void setText(const QString &text) { setPlainText(text); }
    QString text() const { return toPlainText(); }

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void stepBy(double steps);

    int mWheelDeltaRemainder{ };
};

#endif // EXPRESSIONEDITOR_H
