#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <memory>
#include <QJSEngine>
#include "MessageList.h"

class ScriptEngine
{
public:
    void evalScript(const QString &fileName, const QString &source);
    bool operator==(const ScriptEngine &rhs) const { return (mScripts == rhs.mScripts); }
    bool operator!=(const ScriptEngine &rhs) const { return !(*this == rhs); }

    QStringList evalValue(const QStringList &fieldExpressions,
        MessageList &messages);

private:
    struct Script {
        QString fileName;
        QString source;

        friend bool operator==(const Script &a, const Script &b) {
            return (std::tie(a.fileName, a.source) ==
                    std::tie(b.fileName, b.source));
        }
    };

    void evalScripts(MessageList &messages);

    std::vector<Script> mScripts;
    std::unique_ptr<QJSEngine> mJsEngine;
};

#endif // SCRIPTENGINE_H
