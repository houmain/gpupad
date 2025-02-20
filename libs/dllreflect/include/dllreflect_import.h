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
