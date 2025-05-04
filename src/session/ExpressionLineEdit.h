#pragma once

#include <QLineEdit>

class ExpressionLineEdit final : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY(
        QString text READ text WRITE setText NOTIFY textChanged USER true)
public:
    explicit ExpressionLineEdit(QWidget *parent = nullptr);

    void setValue(double value);
    void setText(const QString &text);
    QString text() const;
    bool hasValue(double value) const;

    bool decimal() const { return mDecimal; }
    void setDecimal(bool decimal) { mDecimal = decimal; }

Q_SIGNALS:
    // overridden so base class does not emit
    void textChanged(const QString &);

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void stepBy(int steps);
    void stepBy(double steps);

    int mWheelDeltaRemainder{};
    bool mDecimal{};
};
