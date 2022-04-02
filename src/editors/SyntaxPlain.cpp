
#include "Syntax.h"

class SyntaxPlain : public Syntax {
public:
    bool hasFunctions() const override { return false; }
    QString singleLineCommentBegin() const override { return "^\\s*#.*"; }
    QString multiLineCommentBegin() const override { return ""; }
    QString multiLineCommentEnd() const override { return ""; }
};

Syntax* makeSyntaxPlain() { return new SyntaxPlain(); }
