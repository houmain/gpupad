#ifndef FINDREPLACEBAR_H
#define FINDREPLACEBAR_H

#include <QWidget>
#include <QTextDocument>

namespace Ui {
class FindReplaceBar;
}

class FindReplaceBar : public QWidget
{
    Q_OBJECT
public:
    enum Action {
        FindTextChanged, Find, Replace, ReplaceAll
    };

    explicit FindReplaceBar(QWidget *parent = nullptr);
    ~FindReplaceBar() override;

    void focus(QWidget* target, QString text);
    void cancel();
    QWidget *target() const { return mTarget; }
    void setTarget(QWidget* target);
    void resetTarget();

Q_SIGNALS:
    void action(Action action, QString find, QString replace,
        QTextDocument::FindFlags flags);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void findTextChanged();
    void findNext();
    void findPrevious();
    void replace();
    void replaceAll();
    QTextDocument::FindFlags findFlags() const;

    Ui::FindReplaceBar *ui;
    QWidget *mTarget{ };
};

#endif // FINDREPLACEBAR_H
