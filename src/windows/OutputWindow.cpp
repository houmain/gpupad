#include "OutputWindow.h"
#include "Singletons.h"
#include "Settings.h"
#include "Theme.h"
#include "TitleBar.h"
#include "session/DataComboBox.h"
#include <QScrollBar>
#include <QVBoxLayout>
#include <QPlainTextEdit>

OutputWindow::OutputWindow(QWidget *parent) : QFrame(parent)
    , mTypeSelector(new DataComboBox(this))
    , mTextEdit(new QPlainTextEdit(this))
{
    setFrameShape(QFrame::Box);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mTextEdit);

    mTypeSelector->addItem(tr("Preprocess"), "preprocess");
    mTypeSelector->addItem(tr("Dump SPIR-V"), "spirv");
    mTypeSelector->addItem(tr("Dump glslang AST"), "ast");
    mTypeSelector->addItem(tr("Dump assembly (NV_gpu_program)"), "assembly");
    connect(mTypeSelector, &DataComboBox::currentDataChanged,
        [this](QVariant data) { Q_EMIT typeSelectionChanged(data.toString()); });

    mTextEdit->setFrameShape(QFrame::NoFrame);
    mTextEdit->setReadOnly(true);
    mTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    mTextEdit->setTabStopDistance(
        fontMetrics().horizontalAdvance(QString(2, QChar::Space)));
    mTextEdit->setFont(Singletons::settings().font());

    auto header = new QWidget(this);
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->addWidget(mTypeSelector);
    headerLayout->addStretch(1);

    auto titleBar = new TitleBar();
    titleBar->setWidget(header);
    mTitleBar = titleBar;

    connect(&Singletons::settings(), &Settings::fontChanged,
        mTextEdit, &QPlainTextEdit::setFont);
    connect(&Singletons::settings(), &Settings::windowThemeChanged,
        this, &OutputWindow::handleThemeChanged);
}

QString OutputWindow::selectedType() const
{
    return mTypeSelector->currentData().toString();
}

void OutputWindow::handleThemeChanged(const Theme &theme)
{
    auto palette = theme.palette();
    palette.setColor(QPalette::Base, palette.toolTipBase().color());
    mTextEdit->setPalette(palette);
}

void OutputWindow::setText(QString text)
{
    auto h = mTextEdit->horizontalScrollBar()->value();
    auto v = mTextEdit->verticalScrollBar()->value();
    if (v > 0 || mTextEdit->verticalScrollBar()->maximum() > 0) {
        mLastScrollPosVertical = v;
    }
    else {
        v = mLastScrollPosVertical;
    }

    mTextEdit->setPlainText(text);

    mTextEdit->verticalScrollBar()->setValue(v);
    mTextEdit->horizontalScrollBar()->setValue(h);
}
