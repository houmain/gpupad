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
        &FindReplaceBar::cancel);
}

FindReplaceBar::~FindReplaceBar()
{
    delete ui;
}

void FindReplaceBar::setTarget(QWidget *target)
{
    Q_EMIT action(Action::FindTextChanged, "", "", findFlags());

    mTarget = target;
}

void FindReplaceBar::resetTarget()
{
    mTarget = nullptr;
}

void FindReplaceBar::focus(QWidget *target, QString text)
{
    setTarget(target);
    ui->findText->setText(text);
    ui->findText->selectAll();
    ui->findText->setFocus();
    findTextChanged();
}

void FindReplaceBar::cancel()
{
    Q_EMIT action(Action::FindTextChanged, "", "", findFlags());
    if (mTarget) {
        mTarget->setFocus();
        mTarget = nullptr;
    }
    Q_EMIT cancelled();
}

void FindReplaceBar::findTextChanged()
{
    Q_EMIT action(Action::FindTextChanged, ui->findText->text(), "", findFlags());
}

void FindReplaceBar::findNext()
{
    Q_EMIT action(Action::Find, ui->findText->text(), "", findFlags());
}

void FindReplaceBar::findPrevious()
{
    Q_EMIT action(Action::Find, ui->findText->text(), "",
        findFlags() | QTextDocument::FindBackward);
}

void FindReplaceBar::replace()
{
    Q_EMIT action(Action::Replace, ui->findText->text(),
        ui->replaceText->text(), findFlags());
}

void FindReplaceBar::replaceAll()
{
    Q_EMIT action(Action::ReplaceAll, ui->findText->text(),
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
