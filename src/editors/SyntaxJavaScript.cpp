
#include "Syntax.h"

namespace {

// https://developer.mozilla.org/de/docs/Web/JavaScript/Reference/Lexical_grammar
const auto keywords = QStringList{
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "export", "extends", "finally", "for", "function",
    "if", "import", "in", "instanceof", "new", "return", "super", "switch", "this",
    "throw", "try", "typeof", "var", "void", "while", "with", "yield", "enum",
    "implements", "interface", "let", "package", "private", "protected", "public",
    "static", "await", "of" };

// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects
const auto globalObjects = QStringList{
    "Infinity", "NaN", "undefined", "null", "eval", "isFinite", "isNaN",
    "parseFloat", "parseInt", "decodeURI", "decodeURIComponent", "encodeURI",
    "encodeURIComponent", "Object", "Boolean", "Error", "EvalError", // "Function",
    "RangeError", "ReferenceError", "SyntaxError", "TypeError", "URIError",
    "Number", "Math", "Date", "String", "RegExp", "Array", "Int8Array",
    "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array",
    "Uint32Array", "Float32Array", "Float64Array", "Map", "Set", "WeakMap",
    "WeakSet", "ArrayBuffer", "SharedArrayBuffer", "Atomics", "DataView", "JSON",

    "true", "false", "console", "print" };
} // namespace

class SyntaxJavaScript : public Syntax {
public:
    virtual QStringList keywords() const { return ::keywords; }
    virtual QStringList builtinConstants() const { return ::globalObjects; }
    virtual QStringList completerStrings() const { 
        auto strings = (::keywords + ::globalObjects);
        strings.sort(Qt::CaseInsensitive);
        return strings;
    }
    virtual bool hasFunctions() const { return true; }
    virtual bool hasComments() const { return true; }
};

Syntax* makeSyntaxJavaScript() { return new SyntaxJavaScript(); }