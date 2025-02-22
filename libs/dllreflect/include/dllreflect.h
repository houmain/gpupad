#pragma once

#include <cstdint>

namespace dllreflect {

enum class Type : uint32_t {
  Void,
  Opaque,
  Bool,
  Char,
  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Int64,
  UInt64,
  Float,
  Double,
  Utf8Codepoint,
};

struct ArgumentType {
  Type type;
  const void* opaque_id;
};

struct Argument {
  void* data;
  ArgumentType type;
  uint64_t count;
  void (*free)(void*);
};

struct Function {
  const char* name;
  ArgumentType result_type;
  uint32_t argument_count;
  const ArgumentType* argument_types;
  const void* function;
  void (*call)(const void* function, 
    Argument* result, const Argument* arguments);
};

struct Interface {
  uint32_t function_count;
  const Function* functions;
};

} // namespace
