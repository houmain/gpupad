
#include "Syntax.h"

namespace {

const auto keywords = QStringList{
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function",  "if",
    "in", "local", "nil", "not", "or",
    "repeat", "return", "then", "true", "until", "while"
};

const auto globalObjects = QStringList{
    "assert", "collectgarbage", "dofile", "error", "_G", "getmetatable",
    "ipairs", "load", "loadfile", "next", "pairs", "pcall", "print",
    "rawequal", "rawget", "rawlen", "rawset", "select", "setmetatable",
    "tonumber", "tostring", "type", "_VERSION", "warn", "xpcall",
    "coroutine", "string", "utf8", "table", "math", "io", "os",
    "Session", "Mouse", "Keyboard"
};

} // namespace

class SyntaxLua : public Syntax {
public:
    QStringList keywords() const override { return ::keywords; }
    QStringList builtinConstants() const override { return ::globalObjects; }
    QStringList completerStrings() const override { return ::keywords + ::globalObjects; }
    bool hasFunctions() const override { return true; }
    bool hasComments() const override { return true; }
    QString singleLineCommentBegin() const override { return R"(\-\-.*)"; }
    QString multiLineCommentBegin() const override { return R"(\-\-\[\[)"; }
    QString multiLineCommentEnd() const override { return R"(\]\])"; }
};

Syntax* makeSyntaxLua() { return new SyntaxLua(); }
