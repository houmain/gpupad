#include "OutputWindow.h"
#include "Settings.h"
#include "Singletons.h"
#include "Theme.h"
#include "WindowTitle.h"
#include "widgets/DataComboBox.h"
#include "editors/EditorManager.h"
#include "editors/source/SourceEditor.h"
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

OutputWindow::OutputWindow(QWidget *parent)
    : QFrame(parent)
    , mTypeSelector(new DataComboBox(this))
    , mTextEdit(new QPlainTextEdit(this))
    , mExportButton(new QToolButton(this))
{
    setFrameShape(QFrame::Box);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mTextEdit);

    mTypeSelector->addItem(tr("JSON Reflection"), "json");
    mTypeSelector->addItem(tr("Preprocess"), "preprocess");
    mTypeSelector->addItem(tr("SPIR-V → GLSL"), "glsl");
    mTypeSelector->addItem(tr("SPIR-V → HLSL"), "hlsl");
    mTypeSelector->addItem(tr("SPIR-V"), "spirv");
    mTypeSelector->addItem(tr("glslang AST"), "ast");
    mTypeSelector->addItem(tr("Program Binary (NV_gpu_program)"),
        "programBinary");

    connect(mTypeSelector, &DataComboBox::currentDataChanged,
        [this](QVariant data) {
            Q_EMIT typeSelectionChanged(data.toString());
        });

    mTextEdit->setFrameShape(QFrame::NoFrame);
    mTextEdit->setReadOnly(true);
    mTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    mTextEdit->setTabStopDistance(
        fontMetrics().horizontalAdvance(QString(2, QChar::Space)));
    mTextEdit->setFont(Singletons::settings().font());

    mExportButton->setIcon(
        QIcon(QIcon::fromTheme(QString::fromUtf8("application-exit"))));
    mExportButton->setToolTip(tr("Export To Editor"));
    mExportButton->setAutoRaise(true);
    mExportButton->setEnabled(false);

    auto header = new QWidget(this);
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->addWidget(mTypeSelector);
    headerLayout->addWidget(mExportButton);
    headerLayout->addStretch(1);

    auto titleBar = new WindowTitle();
    titleBar->setWidget(header);
    mTitleBar = titleBar;

    connect(&Singletons::settings(), &Settings::fontChanged, mTextEdit,
        &QPlainTextEdit::setFont);
    connect(&Singletons::settings(), &Settings::windowThemeChanged, this,
        &OutputWindow::handleThemeChanged);
    connect(mExportButton, &QToolButton::clicked, this,
        &OutputWindow::exportText);
}

QString OutputWindow::selectedType() const
{
    return mTypeSelector->currentData().toString();
}

void OutputWindow::handleThemeChanged(const Theme &theme)
{
    auto palette = theme.palette();
    palette.setColor(QPalette::Base, palette.alternateBase().color());
    mTextEdit->setPalette(palette);
}

void OutputWindow::setText(QString text)
{
    auto h = mTextEdit->horizontalScrollBar()->value();
    auto v = mTextEdit->verticalScrollBar()->value();
    if (v > 0 || mTextEdit->verticalScrollBar()->maximum() > 0) {
        mLastScrollPosVertical = v;
    } else {
        v = mLastScrollPosVertical;
    }

    mExportButton->setEnabled(!text.isEmpty());
    mTextEdit->setEnabled(!text.isEmpty());
    mTextEdit->setPlainText(text.isEmpty() ? tr("not available") : text);

    mTextEdit->verticalScrollBar()->setValue(v);
    mTextEdit->horizontalScrollBar()->setValue(h);
}

void OutputWindow::exportText()
{
    auto editor =
        Singletons::editorManager().getSourceEditor(mLastExportFileName);
    if (!editor)
        mLastExportFileName =
            FileDialog::generateNextUntitledFileName("Output");
    editor = Singletons::editorManager().openSourceEditor(mLastExportFileName);
    if (editor)
        editor->replace(mTextEdit->toPlainText());
}
