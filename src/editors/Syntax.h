#pragma once

#include <QStringList>

class Syntax
{
public:
    virtual ~Syntax() = default;
    virtual QStringList keywords() const { return { }; }
    virtual QStringList builtinFunctions() const { return { }; }
    virtual QStringList builtinConstants() const { return { }; }
    virtual QStringList completerStrings() const { return { }; }
    virtual bool hasPreprocessor() const { return false; }
    virtual bool hasFunctions() const { return false; }
    virtual bool hasComments() const { return false; }
    virtual QString singleLineCommentBegin() const { return "//.*"; }
    virtual QString multiLineCommentBegin() const { return "/\\*"; }
    virtual QString multiLineCommentEnd() const { return "\\*/"; }
};

Syntax* makeSyntaxGLSL();
Syntax* makeSyntaxHLSL();
Syntax* makeSyntaxJavaScript();
Syntax* makeSyntaxLua();
