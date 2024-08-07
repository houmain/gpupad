#pragma once

#include <QTextDocument>
#include <QWidget>

namespace Ui {
    class FindReplaceBar;
}

class FindReplaceBar final : public QWidget
{
    Q_OBJECT
public:
    enum Action { FindTextChanged, Find, Replace, ReplaceAll, Refresh, Cancel };

    explicit FindReplaceBar(QWidget *parent = nullptr);
    ~FindReplaceBar() override;

    void setTarget(QWidget *target);
    void setText(const QString &text);
    void focus();
    void resetTarget();
    void cancel();
    QWidget *target() const { return mTarget; }

    void findTextChanged();
    void findNext();
    void findPrevious();
    void replace();
    void replaceAll();
    bool replacing() const { return mReplacing; }

Q_SIGNALS:
    void cancelled();
    void action(FindReplaceBar::Action action, QString find, QString replace,
        QTextDocument::FindFlags flags);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void triggerAction(Action action, QTextDocument::FindFlag extraFlags = {});
    QTextDocument::FindFlags findFlags() const;

    Ui::FindReplaceBar *ui;
    QWidget *mTarget{};
    bool mReplacing{};
};
