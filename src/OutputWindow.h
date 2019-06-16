#ifndef OUTPUTWINDOW_H
#define OUTPUTWINDOW_H

#include <QWidget>

class QPlainTextEdit;
class DataComboBox;

class OutputWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OutputWindow(QWidget *parent = nullptr);

    QString selectedType() const;

signals:
    void typeSelectionChanged(QString type);

public slots:
    void setText(QString text);

private slots:
    void updatePalette();

private:
    DataComboBox *mTypeSelector{ };
    QPlainTextEdit *mTextEdit{ };
    int mLastScrollPosVertical{ };
};

#endif // OUTPUTWINDOW_H
