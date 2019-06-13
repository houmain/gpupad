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
};

#endif // ASSEMBLYWINDOW_H
