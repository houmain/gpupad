#ifndef ASSEMBLYWINDOW_H
#define ASSEMBLYWINDOW_H

#include <QPlainTextEdit>

class AssemblyWindow : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit AssemblyWindow(QWidget *parent = nullptr);

public slots:
    void setText(QString text);

private slots:
    void updatePalette();

private:
    int mLastScrollPosVertical{ };
};

#endif // ASSEMBLYWINDOW_H
