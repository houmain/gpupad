#include "OutputWindow.h"
#include "Singletons.h"
#include "Settings.h"
#include "session/DataComboBox.h"
#include <QScrollBar>
#include <QVBoxLayout>
#include <QPlainTextEdit>

OutputWindow::OutputWindow(QWidget *parent) : QWidget(parent)
    , mTypeSelector(new DataComboBox(this))
    , mTextEdit(new QPlainTextEdit(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mTypeSelector);
    layout->addWidget(mTextEdit);

    mTypeSelector->addItem(tr("Preprocess"), "preprocess");
    mTypeSelector->addItem(tr("Dump SPIR-V"), "spirv");
    mTypeSelector->addItem(tr("Dump glslang AST"), "ast");
    mTypeSelector->addItem(tr("Dump assembly (NV_gpu_program)"), "assembly");
    connect(mTypeSelector, &DataComboBox::currentDataChanged,
        [this](QVariant data) { Q_EMIT typeSelectionChanged(data.toString()); });

    mTextEdit->setReadOnly(true);
    mTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    mTextEdit->setTabStopDistance(
        fontMetrics().horizontalAdvance(QString(2, QChar::Space)));
    mTextEdit->setFont(Singletons::settings().font());

    updatePalette();
    connect(&Singletons::settings(), &Settings::fontChanged,
        mTextEdit, &QPlainTextEdit::setFont);
    connect(&Singletons::settings(), &Settings::darkThemeChanged,
        this, &OutputWindow::updatePalette);
}

QString OutputWindow::selectedType() const
{
    return mTypeSelector->currentData().toString();
}

void OutputWindow::updatePalette()
{
    auto p = palette();
    p.setColor(QPalette::Base, p.toolTipBase().color());
    setPalette(p);
}

void OutputWindow::setText(QString text)
{
    auto h = mTextEdit->horizontalScrollBar()->value();
    auto v = mTextEdit->verticalScrollBar()->value();
    if (v > 0 || mTextEdit->verticalScrollBar()->maximum() > 0)
      mLastScrollPosVertical = v;
    else
      v = mLastScrollPosVertical;

    mTextEdit->setPlainText(text);

    mTextEdit->verticalScrollBar()->setValue(v);
    mTextEdit->horizontalScrollBar()->setValue(h);
}
