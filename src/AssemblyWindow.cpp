#include "AssemblyWindow.h"
#include "Singletons.h"
#include "Settings.h"
#include <QScrollBar>

AssemblyWindow::AssemblyWindow(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(NoWrap);
    setTabStopWidth(2);

    setFont(Singletons::settings().font());
    connect(&Singletons::settings(), &Settings::fontChanged,
        this, &QPlainTextEdit::setFont);

    updatePalette();
    connect(&Singletons::settings(), &Settings::darkThemeChanged,
        this, &AssemblyWindow::updatePalette);
}

void AssemblyWindow::updatePalette()
{
    auto p = palette();
    p.setColor(QPalette::Base, p.toolTipBase().color());
    setPalette(p);
}

void AssemblyWindow::setText(QString text)
{
    auto h = horizontalScrollBar()->value();
    auto v = verticalScrollBar()->value();
    setPlainText(text);
    verticalScrollBar()->setValue(v);
    horizontalScrollBar()->setValue(h);
}
