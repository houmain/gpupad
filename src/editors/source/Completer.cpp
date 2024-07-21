#include "Completer.h"
#include <QRegularExpression>
#include <QStringListModel>

namespace {
    QStringList toList(const QSet<QString> &set)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        return { set.begin(), set.end() };
#else
        return set.toList();
#endif
    }

    QSet<QString> toSet(const QStringList &list)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        return { list.begin(), list.end() };
#else
        return list.toSet();
#endif
    }
} // namespace

Completer::Completer(QObject *parent) : QCompleter(parent)
{
    setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    setCompletionMode(QCompleter::PopupCompletion);
    setCaseSensitivity(Qt::CaseInsensitive);
    setWrapAround(false);
}

void Completer::setStrings(const QStringList &strings)
{
    mStrings = toSet(strings);
    setContextText("");
}

void Completer::setContextText(const QString &contextText)
{
    static const auto pattern = QRegularExpression("[_A-Za-z][_A-Za-z0-9]+");
    auto strings = mStrings;
    for (auto index = contextText.indexOf(pattern); index >= 0;) {
        const auto match = pattern.match(contextText, index);
        strings.insert(match.captured());
        const auto length = match.capturedLength();
        index = contextText.indexOf(pattern, index + length);
    }

    auto list = toList(strings);
    list.sort(Qt::CaseInsensitive);
    delete model();
    setModel(new QStringListModel(list, this));
}
