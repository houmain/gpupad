#ifndef EXPRESSIONMATRIX_H
#define EXPRESSIONMATRIX_H

#include <QTableWidget>

class ExpressionMatrix : public QTableWidget
{
    Q_OBJECT
public:
    explicit ExpressionMatrix(QWidget *parent = nullptr);

    void setRowCount(int rows);
    void setColumnCount(int columns);
    void setValues(QStringList values);
    QStringList values() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void updateCells();
};

#endif // EXPRESSIONMATRIX_H
