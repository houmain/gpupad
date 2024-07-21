#pragma once

#include "SourceType.h"
#include <QRegularExpression>
#include <QSyntaxHighlighter>

class QCompleter;
class Theme;

class SyntaxHighlighter final : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    struct Data;

    SyntaxHighlighter(SourceType sourceType, const Theme &theme,
        bool showWhiteSpace, QObject *parent = nullptr);
    void highlightBlock(const QString &text) override;
    const QStringList &completerStrings() const;

private:
    QSharedPointer<const Data> mData;
};
