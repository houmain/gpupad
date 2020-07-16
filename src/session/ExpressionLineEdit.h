#ifndef EXPRESSIONLINEEDIT_H
#define EXPRESSIONLINEEDIT_H

#include <QLineEdit>

class ExpressionLineEdit : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE
        setText NOTIFY textChanged USER true)
public:
    explicit ExpressionLineEdit(QWidget *parent = nullptr);

    void setText(const QString &text)
    {
        if (text != QLineEdit::text())
            QLineEdit::setText(text);
    }
    QString text() const { return QLineEdit::text(); }

Q_SIGNALS:
    void textChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void stepBy(int steps);

    int mWheelDeltaRemainder{ };
};

#endif // EXPRESSIONLINEEDIT_H
