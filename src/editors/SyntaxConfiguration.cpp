
#include "Syntax.h"

class SyntaxConfiguration : public Syntax {
public:
    bool hasFunctions() const override { return false; }
    QString singleLineCommentBegin() const override { return "^\\s*#.*"; }
    QString multiLineCommentBegin() const override { return ""; }
    QString multiLineCommentEnd() const override { return ""; }
};

Syntax* makeSyntaxConfiguration() { return new SyntaxConfiguration(); }
