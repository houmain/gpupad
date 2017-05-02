#include "FindReplaceBar.h"
#include "ui_FindReplaceBar.h"
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QKeyEvent>

FindReplaceBar::FindReplaceBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FindReplaceBar)
{
    ui->setupUi(this);

    setVisible(false);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::ToolTipBase);

    connect(ui->caseSensitive, &QCheckBox::toggled, this,
        &FindReplaceBar::findTextChanged);
    connect(ui->wholeWordsOnly, &QCheckBox::toggled, this,
        &FindReplaceBar::findTextChanged);
    connect(ui->findText, &QLineEdit::textChanged, this,
        &FindReplaceBar::findTextChanged);
    connect(ui->findText, &QLineEdit::returnPressed, this,
        &FindReplaceBar::findNext);
    connect(ui->replaceText, &QLineEdit::returnPressed, this,
        &FindReplaceBar::replace);
    connect(ui->findNext, &QPushButton::clicked, this,
        &FindReplaceBar::findNext);
    connect(ui->findPrevious, &QPushButton::clicked, this,
        &FindReplaceBar::findPrevious);
    connect(ui->replace, &QPushButton::clicked, this,
        &FindReplaceBar::replace);
    connect(ui->replaceAll, &QPushButton::clicked, this,
        &FindReplaceBar::replaceAll);
    connect(ui->closeButton, &QPushButton::clicked, this,
        &FindReplaceBar::hide);
}

FindReplaceBar::~FindReplaceBar()
{
    delete ui;
}

void FindReplaceBar::setTarget(QWidget *target, QString text)
{
    emit action(Action::FindTextChanged, "", "", findFlags());

    mTarget = target;

    if (text.split(QRegularExpression("\\s"),
            QString::SkipEmptyParts).size() == 1)
        ui->findText->setText(text);

    show();
    ui->findText->selectAll();
    ui->findText->setFocus();
    findTextChanged();
}

void FindReplaceBar::resetTarget()
{
    mTarget = nullptr;
}

void FindReplaceBar::cancel()
{
    emit action(Action::FindTextChanged, "", "", findFlags());

    hide();

    if (mTarget) {
        mTarget->setFocus();
        mTarget = nullptr;
    }
}

void FindReplaceBar::findTextChanged()
{
    emit action(Action::FindTextChanged, ui->findText->text(), "", findFlags());
}

void FindReplaceBar::findNext()
{
    emit action(Action::Find, ui->findText->text(), "", findFlags());
}

void FindReplaceBar::findPrevious()
{
    emit action(Action::Find, ui->findText->text(), "",
        findFlags() | QTextDocument::FindBackward);
}

void FindReplaceBar::replace()
{
    emit action(Action::Replace, ui->findText->text(),
        ui->replaceText->text(), findFlags());
}

void FindReplaceBar::replaceAll()
{
    emit action(Action::ReplaceAll, ui->findText->text(),
        ui->replaceText->text(), findFlags());
}

QTextDocument::FindFlags FindReplaceBar::findFlags() const
{
    auto flags = QTextDocument::FindFlags{ };
    if (ui->caseSensitive->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (ui->wholeWordsOnly->isChecked())
        flags |= QTextDocument::FindWholeWords;
    return flags;
}

void FindReplaceBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        cancel();

    QWidget::keyPressEvent(event);
}
