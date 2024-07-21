#pragma once

#include <QFrame>

class QPlainTextEdit;
class DataComboBox;
class Theme;

class OutputWindow final : public QFrame
{
    Q_OBJECT

public:
    explicit OutputWindow(QWidget *parent = nullptr);
    QWidget *titleBar() const { return mTitleBar; }
    
    QString selectedType() const;
    void setText(QString text);

Q_SIGNALS:
    void typeSelectionChanged(QString type);

private:
    void handleThemeChanged(const Theme &theme);

    QWidget *mTitleBar{ };
    DataComboBox *mTypeSelector{ };
    QPlainTextEdit *mTextEdit{ };
    int mLastScrollPosVertical{ };
};

