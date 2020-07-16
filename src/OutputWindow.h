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
    void setText(QString text);

Q_SIGNALS:
    void typeSelectionChanged(QString type);

private:
    void updatePalette();

    DataComboBox *mTypeSelector{ };
    QPlainTextEdit *mTextEdit{ };
    int mLastScrollPosVertical{ };
};

#endif // OUTPUTWINDOW_H
