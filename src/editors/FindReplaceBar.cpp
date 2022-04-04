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
        [this]() {
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
                findPrevious();
            else
                findNext();
        });
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
    mTarget = target;
}

void FindReplaceBar::resetTarget()
{
    mTarget = nullptr;
}

void FindReplaceBar::setText(const QString &text)
{
    ui->findText->setText(text);
}

void FindReplaceBar::focus() 
{
    ui->findText->selectAll();
    ui->findText->setFocus();
    triggerAction(Action::Refresh);
}

void FindReplaceBar::cancel()
{
    if (mTarget) {
        mTarget->setFocus();
        triggerAction(Action::Cancel);
        setTarget(nullptr);
    }
    Q_EMIT cancelled();
}

void FindReplaceBar::triggerAction(Action type, QTextDocument::FindFlag extraFlags) 
{
    Q_EMIT action(type, ui->findText->text(), 
        ui->replaceText->text(), findFlags() | extraFlags);
}

void FindReplaceBar::findTextChanged()
{
    triggerAction(Action::FindTextChanged);
}

void FindReplaceBar::findNext()
{
    triggerAction(Action::Find);
}

void FindReplaceBar::findPrevious()
{
    triggerAction(Action::Find, QTextDocument::FindBackward);
}

void FindReplaceBar::replace()
{
    mReplacing = true;
    triggerAction(Action::Replace);
    mReplacing = false;
}

void FindReplaceBar::replaceAll()
{
    mReplacing = true;
    triggerAction(Action::ReplaceAll);
    mReplacing = false;
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
