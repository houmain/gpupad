#pragma once

#include <cstdint>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if (__cplusplus >= 202002L)
# include <span>
# define DLLREFLECT_CXX20
#endif

namespace dllreflect {

enum class Type : uint32_t {
  Void,
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

enum class TypeFlags : uint32_t {
  opaque = (1 << 20),
  array  = (1 << 21),
  sink   = (1 << 22),
};

struct Argument {
  void* data;
  Type type;
  uint64_t count;
  void (*free)(void*);
};

struct Function {
  const char* name;
  Type result_type;
  uint32_t argument_count;
  const Type* argument_types;
  const void* function;
  void (*call)(const void* function, 
    Argument* result, const Argument* arguments);
};

struct Interface {
  uint32_t function_count;
  const Function* functions;
};

class Opaque : public Argument {
public:
  Opaque() = default;
  Opaque(const Opaque& rhs) = delete;
  Opaque& operator=(const Opaque& rhs) = delete;
  Opaque(Opaque&& rhs) noexcept
    : Argument(static_cast<Argument&>(rhs)) {
    static_cast<Argument&>(rhs) = { };
  }
  Opaque& operator=(Opaque&& rhs) noexcept {
    auto tmp = std::move(rhs);
    std::swap(
      static_cast<Argument&>(*this), 
      static_cast<Argument&>(tmp));
    return *this;
  }
  ~Opaque() {
    if (free)
      free(data);
  }
};

constexpr TypeFlags operator|(TypeFlags type, TypeFlags flags) {
  return static_cast<TypeFlags>(static_cast<uint32_t>(type) | static_cast<uint32_t>(flags));
}

constexpr Type operator|(Type type, TypeFlags flags) {
  return static_cast<Type>(static_cast<uint32_t>(type) | static_cast<uint32_t>(flags));
}

constexpr bool operator&(Type type, TypeFlags flags) {
  return static_cast<bool>(static_cast<uint32_t>(type) & static_cast<uint32_t>(flags));
}

constexpr Type base(Type type) {
  // opaque is not removed
  return static_cast<Type>(static_cast<uint32_t>(type) &
      ~static_cast<uint32_t>(TypeFlags::array | TypeFlags::sink));
}

namespace detail {
  // source: https://stackoverflow.com/questions/36117884/compile-time-typeid-for-every-type/77093119#77093119
  template<auto Id>
  struct counter {
    using tag = counter;

    struct generator {
      template<typename...>
      friend constexpr auto is_defined(tag) { return true; }
    };

    template<typename...>
    friend constexpr auto is_defined(tag);

    template<typename Tag = tag, auto I = (int)is_defined(Tag{}) >
    static constexpr auto exists(decltype(I)) { return true; }

    static constexpr auto exists(...) { return generator(), false; }
  };

  template<typename T, auto Id = int{} >
  constexpr auto unique_id() {
    if constexpr (counter<Id>::exists(Id)) {
      return unique_id<T, Id + 1>();
    }
    else {
      return Id;
    }
  }

  template<typename T>
  struct get_type {
    constexpr static Type type = (static_cast<Type>(unique_id<T>()) | TypeFlags::opaque);
  };
  template<> constexpr Type get_type<void>::type = Type::Void;
  template<> constexpr Type get_type<bool>::type = Type::Bool;
  template<> constexpr Type get_type<char>::type = Type::Char;
  template<> constexpr Type get_type<int8_t>::type = Type::Int8;
  template<> constexpr Type get_type<uint8_t>::type = Type::UInt8;
  template<> constexpr Type get_type<int16_t>::type = Type::Int16;
  template<> constexpr Type get_type<uint16_t>::type = Type::UInt16;
  template<> constexpr Type get_type<int32_t>::type = Type::Int32;
  template<> constexpr Type get_type<uint32_t>::type = Type::UInt32;
  template<> constexpr Type get_type<int64_t>::type = Type::Int64;
  template<> constexpr Type get_type<uint64_t>::type = Type::UInt64;
  template<> constexpr Type get_type<float>::type = Type::Float;
  template<> constexpr Type get_type<double>::type = Type::Double;

  template<> constexpr Type get_type<std::string>::type = (Type::Utf8Codepoint | TypeFlags::array);
  template<> constexpr Type get_type<std::string_view>::type = (Type::Utf8Codepoint | TypeFlags::array);
  template<> constexpr Type get_type<const char*>::type = (Type::Utf8Codepoint | TypeFlags::array);

  template<typename T>
  constexpr inline Type get_type_v = get_type<std::decay_t<T>>::type;

  template<typename T, typename A>
  struct get_type<std::vector<T, A>> {
    constexpr static Type type = (get_type_v<T> | TypeFlags::array);
  };

  template<typename T>
  struct get_argument {
    constexpr static T get(const Argument& argument) {
      return *static_cast<const std::decay_t<T>*>(argument.data);
    }
  };

  template<typename T>
  struct get_argument<std::vector<T>> {
    constexpr static std::vector<T> get(const Argument& argument) {
      const auto begin = static_cast<const T*>(argument.data);
      const auto end = begin + argument.count;
      return { begin, end };
    }
  };

  template<> 
  struct get_argument<std::string> {
    static std::string get(const Argument& argument) {
      return { static_cast<const char*>(argument.data), argument.count };
    }
  };

  template<> 
  struct get_argument<const std::string&> {
    static std::string get(const Argument& argument) {
      return { static_cast<const char*>(argument.data), argument.count };
    }
  };

  template<> 
  struct get_argument<std::string_view> {
    static std::string_view get(const Argument& argument) {
      return { static_cast<const char*>(argument.data), argument.count };
    }
  };

  template<> 
  struct get_argument<const char*> {
    static const char* get(const Argument& argument) {
      return static_cast<const char*>(argument.data);
    }
  };

  template<typename T>
  void write_result(Argument& result, T&& value) {
    if (result.type == get_type_v<T>) {
      *static_cast<T*>(result.data) = std::forward<T>(value);
    }
    else {
      result.type = get_type_v<T>;
      result.count = 1;
      result.data = new T(std::forward<T>(value));
      result.free = [](void* ptr) { delete static_cast<T*>(ptr); };
    }
  }

  template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  void write_result(Argument& result, std::vector<T>&& values) {
    result.type = get_type_v<std::vector<T>>;
    result.count = values.size();
    result.data = new T[result.count];
    result.free = [](void* ptr) { delete[] static_cast<T*>(ptr); };
    std::memcpy(result.data, values.data(), result.count * sizeof(T));
  }

  void write_result(Argument& result, std::string&& value) {
    result.type = get_type_v<std::string>;
    result.count = value.size();
    result.data = new char[result.count];
    result.free = [](void* ptr) { delete[] static_cast<char*>(ptr); };
    std::memcpy(result.data, value.data(), result.count);
  }

  void write_void_result(Argument& result) {
    result.type = { Type::Void };
  }

  template<typename Func, typename R, typename... Args, std::size_t... Is>
  R call_with_args(Func func, std::index_sequence<Is...>, const Argument* arguments) {
    return func(get_argument<Args>::get(arguments[Is])...);
  }

  template<typename... Args>
  struct TypeList {
    static constexpr Type types[std::max(size_t{ 1 }, sizeof...(Args))]{ get_type_v<Args>... };
  };

  constexpr size_t get_type_alignment(Type type) {
    switch (type) {
      case Type::Void: return 0;
      case Type::Bool:
      case Type::Char:
      case Type::Int8:
      case Type::UInt8:
      case Type::Utf8Codepoint: return 1;
      case Type::Int16:
      case Type::UInt16: return 2;
      case Type::Int32:
      case Type::UInt32:
      case Type::Float: return 4;
      case Type::Int64:
      case Type::UInt64:
      case Type::Double: return 8;
    }
    return 0;
  }

  template<typename T>
  struct get_array_data {
    static std::pair<const void*, size_t> get(const T& array) {
      return { array.data(), array.size() };
    }
  };

  template<>
  struct get_array_data<const char*> {
    static std::pair<const void*, size_t> get(const char* array) {
      return { array, std::strlen(array) };
    }
  };
} // namespace detail

template<typename R, typename... Args>
constexpr Function describe(const char* name, R(*function)(Args...)) {
  using namespace detail;
  return Function{
    name,
    get_type_v<R>,
    sizeof...(Args),
    TypeList<Args...>::types,
    function,
    [](const void* _func, Argument* result, const Argument* arguments) {
      auto func = static_cast<R(*)(Args...)>(_func);
      if constexpr(std::is_same_v<R, void>) {
        call_with_args<decltype(func), R, Args...>(func, 
          std::index_sequence_for<Args...>{}, arguments);
        if (result)
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

class Library {
public:
  Library() = default;
  ~Library() noexcept;
  Library(Library&& rhs) noexcept;
  Library& operator=(Library&& rhs) noexcept;

#if defined(_WIN32)
  bool load(const std::wstring& directory, const std::wstring& name) noexcept;
#else // !_WIN32
  bool load(const std::string& directory, const std::string& name) noexcept;
#endif
  void unload() noexcept;
  bool loaded() const { return static_cast<bool>(m_handle); }
  const Interface& interface() const { return m_interface; }

private:
  template<typename Signature>
  Signature* get_function(const char* name) noexcept {
    return reinterpret_cast<Signature*>(get_function_impl(name));
  }
  void* get_function_impl(const char* name) noexcept;

  void* m_handle{ };
  Interface m_interface{ };
};

struct CallBuffer {
  std::vector<std::byte> data;
  std::vector<Argument> args;
};

template<typename R, typename... Args>
R call(const Function& function, Args&&... args) {
  using namespace detail;

  if (function.argument_count != sizeof...(Args))
    throw std::runtime_error("argument count mismatch");

  Argument arguments[std::max(sizeof...(Args), size_t{ 1 })];
  auto index = 0;
  ([&]{
    if constexpr (std::is_same_v<std::decay_t<Args>, Opaque>) {
      const auto type = args.type;
      if (function.argument_types[index] != type)
        throw std::runtime_error("argument type mismatch");
      arguments[index++] = args;
    }
    else {
      const auto type = get_type_v<Args>;
      if (function.argument_types[index] != type)
        throw std::runtime_error("argument type mismatch");

      if constexpr (type & TypeFlags::array) {
        const auto [data, size] = get_array_data<std::decay_t<Args>>::get(args);
        arguments[index++] = { 
          const_cast<void*>(data),
          type,
          static_cast<uint64_t>(size)
        };
      }
      else {  
        arguments[index++] = { &args, type, 1 };
      }
    }
  }(), ...);

  if constexpr (std::is_same_v<R, void>) {
    if (function.result_type != Type::Void)
      throw std::runtime_error("result type mismatch");
    function.call(function.function, nullptr, arguments);
  }
  else {
    auto result_value = R{ };
    if constexpr (std::is_same_v<R, Opaque>) {
      if (!(function.result_type & TypeFlags::opaque))
        throw std::runtime_error("result type mismatch");
    
      // write directly in result_value, which is an Argument
      function.call(function.function, &result_value, arguments);
    }
    else {
      const auto type = get_type_v<R>;
      if (type != function.result_type)
        throw std::runtime_error("result type mismatch");

      if constexpr (type & TypeFlags::array) {
        // construct array from heap allocation
        auto result = Argument{ };
        function.call(function.function, &result, arguments);
        using T = typename R::value_type;
        result_value = R{ 
          static_cast<const T*>(result.data), 
          static_cast<size_t>(result.count)
        };
        if (result.free)
          result.free(result.data);
      }
      else {
        // write directly in result_value
        auto result = Argument{ &result_value, function.result_type, 1 };
        function.call(function.function, &result, arguments);
      }
    }
    return result_value;
  }
}

template <
  typename GetArgument, // bool(size_t index, Argument& argument)
  typename WriteResult> // bool(Argument& argument)
bool call(const Function &function,
    const GetArgument &get_argument,
    const WriteResult &write_result,
    CallBuffer *buffer = nullptr) {
  using namespace detail;

  auto new_buffer = CallBuffer{};
  if (!buffer)
    buffer = &new_buffer;

  const auto advance = [](size_t &pos, size_t alignment, size_t count) {
    if (alignment > 0 && count > 0)
      pos = ((pos + alignment - 1) / alignment + count) * alignment;
  };

  // allocate count elements in argument buffer
  buffer->args.resize(function.argument_count);
  auto buffer_size = size_t{};
  for (auto i = 0u; i < function.argument_count; ++i) {
    auto &arg = buffer->args[i];
    arg.type = function.argument_types[i];
    arg.count = (arg.type & (TypeFlags::array | TypeFlags::opaque) ? 0 : 1);
    advance(buffer_size, get_type_alignment(arg.type), arg.count);
  }

  // call get_argument for each argument
  // it should either write to data pointing to argument buffer
  // or allocate itself and set data/free/count
  buffer->data.resize(buffer_size);
  auto succeeded = true;
  auto pos = size_t{};
  for (auto i = 0u; i < function.argument_count; ++i) {
    auto &arg = buffer->args[i];
    arg.data = (arg.count > 0 ? buffer->data.data() + pos : nullptr);
    arg.free = nullptr;
    advance(pos, get_type_alignment(arg.type), arg.count);
    succeeded = (succeeded && get_argument(i, arg));
  }

  // allocate some bytes for result on the stack
  auto result_buffer = uint64_t{};
  auto result = Argument{};
  const auto result_size = get_type_alignment(function.result_type);
  if (result_size > 0 && result_size <= sizeof(result_buffer)) {
    result.data = &result_buffer;
    result.type = function.result_type;
    result.count = 1;
  }

  if (succeeded)
    function.call(function.function, &result, buffer->args.data());

  // write result, should reset free 
  // when it takes ownership of the allocation
  succeeded = (succeeded && write_result(result));

  if (result.free)
    result.free(result.data);

  return succeeded;
}

} // namespace

#if defined(_WIN32)
#  define DLLREFLECT_API extern "C" __declspec(dllexport)
#else
#  define DLLREFLECT_API extern "C" __attribute__ ((visibility ("default")))
#endif

#define DLLREFLECT_BEGIN(...) \
  DLLREFLECT_API const dllreflect::Interface* dllreflect_export() { \
    __VA_ARGS__; \
    static const dllreflect::Function s_functions[] = {

#define DLLREFLECT_FUNC(NAME) dllreflect::describe(#NAME, &NAME),

#define DLLREFLECT_END() \
    }; \
    static const dllreflect::Interface s_interface{ \
      sizeof(s_functions) / sizeof(dllreflect::Function), \
      s_functions, \
    }; \
    return &s_interface; \
  }

#if defined(DLLREFLECT_IMPORT_IMPLEMENTATION)
#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#else
# include <dlfcn.h>
#endif

namespace dllreflect {

Library::Library(Library&& rhs) noexcept
  : m_handle(std::exchange(rhs.m_handle, nullptr)),
    m_interface(std::exchange(rhs.m_interface, { })) {
}

Library& Library::operator=(Library&& rhs) noexcept {
  auto tmp = std::move(rhs);
  std::swap(tmp.m_handle, m_handle);
  std::swap(tmp.m_interface, m_interface);
  return *this;
}

Library::~Library() noexcept {
  unload();
}

#if defined(_WIN32)
bool Library::load(const std::wstring& directory, const std::wstring& name) noexcept {
  SetDllDirectoryW(directory.c_str());
  m_handle = LoadLibraryW(name.c_str());
#else // !_WIN32
bool Library::load(const std::string& directory, const std::string& name) noexcept {
  const auto filename = "./" + directory + "/" + name + ".so";
  m_handle = dlopen(filename.c_str(), RTLD_LAZY);
#endif // !_WIN32
  if (m_handle == nullptr)
    return false;

  if (auto get_interface = get_function<const Interface*()>("dllreflect_export"))
    if (auto interface = get_interface())
      m_interface = *interface;
  
  return true;
}

void Library::unload() noexcept {
  if (!m_handle)
    return;
#if defined(_WIN32)
  FreeLibrary(static_cast<HMODULE>(m_handle));
#else
  dlclose(m_handle);
#endif
  m_handle = nullptr;
  m_interface = { };
}

void* Library::get_function_impl(const char* name) noexcept {
  if (!m_handle)
    return nullptr;
#if defined(_WIN32)
  return GetProcAddress(static_cast<HMODULE>(m_handle), name);
#else
  return dlsym(m_handle, name);
#endif
}

} // namespace

#endif // DLLREFLECT_IMPORT_IMPLEMENTATION
