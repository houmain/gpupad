
#include "Syntax.h"

namespace {

// https://developer.mozilla.org/de/docs/Web/JavaScript/Reference/Lexical_grammar
const auto keywords = QStringList{
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "export", "extends", "finally", "for", "function",
    "if", "import", "in", "instanceof", "new", "return", "super", "switch", "this",
    "throw", "try", "typeof", "var", "void", "while", "with", "yield", "enum",
    "implements", "interface", "let", "package", "private", "protected", "public",
    "static", "await", "of", 
    "Infinity", "NaN", "undefined", "null", "true", "false"
};

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
const auto globalObjects = QStringList{
    "eval", "isFinite", "isNaN",
    "parseFloat", "parseInt", "decodeURI", "decodeURIComponent", "encodeURI",
    "encodeURIComponent", "Object", "Boolean", "Error", "EvalError", "Function",
    "RangeError", "ReferenceError", "SyntaxError", "TypeError", "URIError",
    "Number", "Math", "Date", "String", "RegExp", "Array", "Int8Array",
    "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array",
    "Uint32Array", "Float32Array", "Float64Array", "Map", "Set", "WeakMap",
    "WeakSet", "ArrayBuffer", "SharedArrayBuffer", "Atomics", "DataView", "JSON",
    "console", "gpupad", "Mouse", "Keyboard"
};

} // namespace

class SyntaxJavaScript : public Syntax {
public:
    QStringList keywords() const override { return ::keywords; }
    QStringList builtinConstants() const override { return ::globalObjects; }
    QStringList completerStrings() const override { return ::keywords + ::globalObjects; }
    bool hasFunctions() const override { return true; }
    bool hasComments() const override { return true; }
};

Syntax* makeSyntaxJavaScript() { return new SyntaxJavaScript(); }
