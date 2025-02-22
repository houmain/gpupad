#pragma once

#include "dllreflect.h"
#include <string>
#include <vector>

namespace dllreflect {

namespace detail {
  template<typename T>
  struct OpaqueTypeId { constexpr static int s = 0; };

  constexpr bool operator==(const ArgumentType& a, const ArgumentType& b)  {
    return std::tie(a.type, a.opaque_id) == std::tie(b.type, b.opaque_id);
  }

  template<typename T> constexpr ArgumentType get_argument_type() {
    return { Type::Opaque, &OpaqueTypeId<T>::s };
  }
  template<> constexpr ArgumentType get_argument_type<void>() { return { Type::Void }; }
  template<> constexpr ArgumentType get_argument_type<int8_t>() { return { Type::Int8 }; }
  template<> constexpr ArgumentType get_argument_type<uint8_t>() { return { Type::UInt8 }; }
  template<> constexpr ArgumentType get_argument_type<int16_t>() { return { Type::Int16 }; }
  template<> constexpr ArgumentType get_argument_type<uint16_t>() { return { Type::UInt16 }; }
  template<> constexpr ArgumentType get_argument_type<int32_t>() { return { Type::Int32 }; }
  template<> constexpr ArgumentType get_argument_type<uint32_t>() { return { Type::UInt32 }; }
  template<> constexpr ArgumentType get_argument_type<int64_t>() { return { Type::Int64 }; }
  template<> constexpr ArgumentType get_argument_type<uint64_t>() { return { Type::UInt64 }; }
  template<> constexpr ArgumentType get_argument_type<float>() { return { Type::Float }; }
  template<> constexpr ArgumentType get_argument_type<double>() { return { Type::Double }; }
  template<> constexpr ArgumentType get_argument_type<std::string>() { return { Type::Utf8Codepoint }; }

  template<typename T>
  constexpr T get_argument(const Argument& argument) {
    return *static_cast<const T*>(argument.data);
  }

  template<>
  std::string get_argument<std::string>(const Argument& argument) {
    return static_cast<const char*>(argument.data);
  }

  template<typename T>
  void write_result(Argument& result, const T& value) {
    if (result.type == get_argument_type<T>()) {
      *static_cast<T*>(result.data) = value;
    }
    else {
      result.type = get_argument_type<T>();
      result.count = 1;
      result.data = new T(value);
      result.free = [](void* ptr) { delete static_cast<T*>(ptr); };
    }
  }

  void write_result(Argument& result, const std::string& value) {
    result.type = get_argument_type<std::string>();
    result.count = (value.size() + 1);
    result.data = new char[result.count];
    result.free = [](void* ptr) { delete[] static_cast<char*>(ptr); };
    std::memcpy(result.data, value.data(), result.count);
  }

  void write_void_result(Argument& result) {
    result.type = { Type::Void };
  }

  template<typename Func, typename R, typename... Args, std::size_t... Is>
  R call_with_args(Func func, std::index_sequence<Is...>, const Argument* arguments) {
    return func(get_argument<Args>(arguments[Is])...);
  }

  template<typename... Args>
  struct ArgumentTypeList {
    static constexpr ArgumentType types[std::max(size_t{ 1 }, sizeof...(Args))]{ get_argument_type<Args>()... };
  };
} // namespace detail

template<typename R, typename... Args>
constexpr Function describe(const char* name, R(*function)(Args...)) {
  using namespace detail;
  return Function{
    name,
    get_argument_type<R>(),
    sizeof...(Args),
    ArgumentTypeList<Args...>::types,
    function,
    [](const void* _func, Argument* result, const Argument* arguments) {
      auto func = static_cast<R(*)(Args...)>(_func);
      if constexpr(std::is_same_v<R, void>) {
        call_with_args<decltype(func), R, Args...>(func, 
          std::index_sequence_for<Args...>{}, arguments);
        write_void_result(*result);
      }
      else {
        write_result(*result, 
          call_with_args<decltype(func), R, Args...>(func, 
            std::index_sequence_for<Args...>{}, arguments));
      }
    }
  };
}

} // namespace

#if defined(_WIN32)
#  define DLLREFLECT_API extern "C" __declspec(dllexport)
#else
#  define DLLREFLECT_API extern "C" __attribute__ ((visibility ("default")))
#endif

#define DLLREFLECT_BEGIN \
  DLLREFLECT_API dllreflect::Interface dllreflect_export() { \
    static const dllreflect::Function functions[] = {

#define DLLREFLECT_FUNC(NAME) dllreflect::describe(#NAME, &NAME),

#define DLLREFLECT_END \
    }; \
    return { \
      sizeof(functions) / sizeof(dllreflect::Function), \
      functions, \
    }; \
  }
