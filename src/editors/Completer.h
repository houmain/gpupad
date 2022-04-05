#pragma once

#include <QCompleter>
#include <QSet>

class Completer : public QCompleter
{
    Q_OBJECT
public:
    explicit Completer(QObject *parent = nullptr);
    void setStrings(const QStringList &strings);
    void setContextText(const QString &contextText);

private:
    QSet<QString> mStrings;
};
