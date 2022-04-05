#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include "SourceType.h"

class QCompleter;

class SyntaxHighlighter final : public QSyntaxHighlighter 
{
    Q_OBJECT
public:
    struct Data;

    SyntaxHighlighter(SourceType sourceType, 
        bool darkTheme, bool showWhiteSpace, QObject *parent = nullptr);
    void highlightBlock(const QString &text) override;
    const QStringList &completerStrings() const;

private:
    QSharedPointer<const Data> mData;
};
