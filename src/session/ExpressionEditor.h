#ifndef EXPRESSIONEDITOR_H
#define EXPRESSIONEDITOR_H

#include <QPlainTextEdit>

class ExpressionEditor : public QPlainTextEdit
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE
        setText NOTIFY textChanged USER true)
public:
    explicit ExpressionEditor(QWidget *parent = nullptr);

    void setText(const QString &text)
    {
        if (text != toPlainText())
            setPlainText(text);
    }
    QString text() const { return toPlainText(); }

    using QAbstractScrollArea::setViewportMargins;

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    void stepBy(double steps);

    int mWheelDeltaRemainder{ };
};

#endif // EXPRESSIONEDITOR_H
