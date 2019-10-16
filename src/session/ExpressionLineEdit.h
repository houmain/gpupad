#ifndef EXPRESSIONLINEEDIT_H
#define EXPRESSIONLINEEDIT_H

#include <QLineEdit>

class ExpressionLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit ExpressionLineEdit(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void stepBy(int steps);

    int mWheelDeltaRemainder{ };
};

#endif // EXPRESSIONLINEEDIT_H
