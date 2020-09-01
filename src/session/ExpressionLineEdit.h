#ifndef EXPRESSIONLINEEDIT_H
#define EXPRESSIONLINEEDIT_H

#include <QLineEdit>

class ExpressionLineEdit final : public QLineEdit
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

    bool decimal() const { return mDecimal; }
    void setDecimal(bool decimal) { mDecimal = decimal; }

Q_SIGNALS:
    void textChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void stepBy(int steps);
    void stepBy(double steps);

    int mWheelDeltaRemainder{ };
    bool mDecimal{ };
};

#endif // EXPRESSIONLINEEDIT_H
