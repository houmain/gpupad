#ifndef EXPRESSIONMATRIX_H
#define EXPRESSIONMATRIX_H

#include <QTableWidget>

class ExpressionMatrix : public QTableWidget
{
public:
    explicit ExpressionMatrix(QWidget *parent = 0);

    void setRowCount(int rows);
    void setColumnCount(int columns);
    void setFields(QStringList fields);
    QStringList fields() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void updateCells();
};

#endif // EXPRESSIONMATRIX_H
