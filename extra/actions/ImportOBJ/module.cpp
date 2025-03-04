
#include "dllreflect.h"
#include "rapidobj.hpp"
#include <span>

using Model = rapidobj::Result;

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
  auto result = ParseFile(utf8_to_path(filename), material);
  Triangulate(result);
  return result;
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

std::string getError(const Model& model) {
  if (!model.error.code)
    return "";
  return model.error.code.message();
}

size_t getShapeCount(const Model& model) {
  return model.shapes.size();
}

std::vector<float> getShapeMeshVertices(const Model& model, size_t index) {
  if (index >= model.shapes.size())
    return { };
  const auto& mesh = model.shapes[index].mesh;
  const auto positions = model.attributes.positions.data();
  const auto normals = model.attributes.normals.data();
  const auto texcoords = model.attributes.texcoords.data();

  auto result = std::vector<float>();
  result.reserve(mesh.indices.size() * 8);
  for (auto i = 0; i < mesh.indices.size(); i += 3) {
    const auto indices = &mesh.indices[i];
    for (auto j = 0; j < 3; ++j)
      result.push_back(positions[indices[j].position_index]);
    for (auto j = 0; j < 3; ++j)
      result.push_back((indices[j].normal_index >= 0 ? 
        (normals[indices[j].normal_index]) : 0));
    for (auto j = 0; j < 2; ++j)
      result.push_back((indices[j].texcoord_index >= 0 ? 
        texcoords[indices[j].texcoord_index] : 0));
  }
  return result;
}

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(loadFile)
DLLREFLECT_FUNC(getError)
DLLREFLECT_FUNC(getShapeCount)
DLLREFLECT_FUNC(getShapeMeshVertices)
DLLREFLECT_END()
