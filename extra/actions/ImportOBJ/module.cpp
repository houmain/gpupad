
#include "dllreflect.h"
#include "rapidobj.hpp"
#include <span>

std::filesystem::path utf8_to_path(std::string_view utf8_string) {
#if defined(__cpp_char8_t)
  static_assert(sizeof(char) == sizeof(char8_t));
  return std::filesystem::path(
    reinterpret_cast<const char8_t*>(utf8_string.data()),
    reinterpret_cast<const char8_t*>(utf8_string.data() + utf8_string.size()));
#else
  return std::filesystem::u8path(utf8_string);
#endif
}

rapidobj::Result loadFile(const std::string& filename) noexcept try {
  using namespace rapidobj;
  auto material = MaterialLibrary::SearchPath(".", Load::Optional);
  return ParseFile(utf8_to_path(filename), material);
}
catch (const std::exception& ex) {
  auto result = rapidobj::Result{};
  result.error = { 
    std::make_error_code(std::io_errc::stream),
    ex.what(),
    0
  };
  return result;
}

std::string getError(const rapidobj::Result& result) {
  if (!result.error.code)
    return "";
  return result.error.code.message();
}

bool triangulate(rapidobj::Result& result) {
  return Triangulate(result);
}

size_t getShapeCount(const rapidobj::Result& result) {
  return result.shapes.size();
}

std::span<const rapidobj::Index> getShapeMeshIndices(
    const rapidobj::Result& result, size_t index) {
  if (index >= result.shapes.size())
    return { };
  const auto& indices = result.shapes[index].mesh.indices;
  return { indices.data(), indices.size() };
}

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(loadFile)
DLLREFLECT_FUNC(getError)
//DLLREFLECT_FUNC(triangulate)
DLLREFLECT_FUNC(getShapeCount)
//DLLREFLECT_FUNC(getShapeMeshIndices)
DLLREFLECT_END()
