#pragma once

#include "dllreflect.h"
#include <string>
#include <utility>

namespace dllreflect {

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

constexpr size_t get_type_alignment(Type type) {
  switch (type) {
    case Type::Void:
    case Type::Opaque: return 0;
    case Type::Bool:
    case Type::Char:
    case Type::Int8:
    case Type::UInt8: return 1;
    case Type::Int16:
    case Type::UInt16: return 2;
    case Type::Int32:
    case Type::UInt32: return 4;
    case Type::Int64:
    case Type::UInt64: return 8;
    case Type::Float: return 4;
    case Type::Double: return 8;
    case Type::Utf8Codepoint: return 1;
  }
  return 0;
}

template <
  typename GetArgumentSize, // size_t(size_t index, ArgumentType type)
  typename GetArgument, // bool(size_t index, Argument& argument)
  typename WriteResult> // bool(Argument& argument)
bool make_call(const Function &function,
    GetArgumentSize &get_argument_size,
    GetArgument &get_argument,
    WriteResult &write_result,
    CallBuffer *buffer = nullptr) {

  auto new_buffer = CallBuffer{};
  if (!buffer)
    buffer = &new_buffer;

  const auto advance = [](size_t &pos, size_t alignment, size_t count) {
    if (alignment > 0 && count > 0)
      pos = ((pos + alignment - 1) / alignment + count) * alignment;
  };

  // call get_argument_size for each argument to get number of elements
  // to allocate in argument buffer
  // it should return 0 when it allocates itself
  buffer->args.resize(function.argument_count);
  auto buffer_size = size_t{};
  for (auto i = 0u; i < function.argument_count; ++i) {
    auto &arg = buffer->args[i];
    arg.type = function.argument_types[i];
    arg.count = get_argument_size(i, arg.type.type);
    advance(buffer_size, get_type_alignment(arg.type.type), arg.count);
  }

  // call get_argument for each argument
  // it should either write to data pointing to requested buffer
  // or allocate itself and set data/free/count
  buffer->data.resize(buffer_size);
  auto succeeded = true;
  auto pos = size_t{};
  for (auto i = 0u; i < function.argument_count; ++i) {
    auto &arg = buffer->args[i];
    arg.data = (arg.count > 0 ? buffer->data.data() + pos : nullptr);
    arg.free = nullptr;
    advance(pos, get_type_alignment(arg.type.type), arg.count);
    succeeded = (succeeded && get_argument(i, arg));
  }

  // allocate some bytes for result on the stack
  auto result_buffer = uint64_t{};
  auto result = Argument{};
  const auto result_size = get_type_alignment(function.result_type.type);
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
    m_interface(std::exchange(rhs.m_interface, Interface{ })) {
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

  if (auto get_interface = get_function<Interface()>("dllreflect_export"))
    m_interface = get_interface();
  
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
