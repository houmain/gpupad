
#include "dllreflect.h"
#include "rapidobj.hpp"
#include "jsonc.c"
#include <span>

namespace {
  struct Settings {
    float width{ 2 };
    float height{ 2 };
    float depth{ 2 };
    float scale_u{ 1 };
    float scale_v{ 1 };
    bool normalize{ };
    bool center{ };
    bool swap_yz{ };
  };

  struct AABB {
    float min[3];
    float max[3];
  };

  struct Model {
    Settings settings;
    rapidobj::Result object;
    AABB aabb;
    std::string error;
  };

  void parse_json(float& value, const std::string& json, const char* name) {
    if (auto string = json_get(json.c_str(), name)) {
      value = static_cast<float>(std::atof(string));
      free(string);
    }
  }

  void parse_json(bool& value, const std::string& json, const char* name) {
    if (auto string = json_get(json.c_str(), name)) {
      value = (std::strcmp(string, "true") == 0);
      free(string);
    }
  }

  Settings parse_settings(const std::string& json) {
    auto s = Settings{ };
    parse_json(s.width, json, "width");
    parse_json(s.height, json, "height");
    parse_json(s.depth, json, "depth");
    parse_json(s.scale_u, json, "scaleU");
    parse_json(s.scale_v, json, "scaleV");
    parse_json(s.normalize, json, "normalize");
    parse_json(s.center, json, "center");
    parse_json(s.swap_yz, json, "swapYZ");
    return s;
  }

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

  AABB compute_aabb(const Model& model) {
    auto aabb = AABB{ };
    for (auto c = 0; c < 3; ++c) {
      aabb.min[c] = std::numeric_limits<float>::max();
      aabb.max[c] = std::numeric_limits<float>::lowest();
    }

    auto i = 0u;
    for (const auto& position : model.object.attributes.positions) {
      const auto c = i % 3;
      aabb.min[c] = std::min(aabb.min[c], position);
      aabb.max[c] = std::max(aabb.max[c], position);
      ++i;
    }
    return aabb;
  }

  float get_max_dimension(const AABB& aabb) {
    auto max = 0.0f;
    for (auto i = 0; i < 3; i++)
      max = std::max(aabb.max[i] - aabb.min[i], max);
    return max;
  }

  void transform_vertices(float* vertices, size_t size, const Model& model) {
    const auto& settings = model.settings;
    const auto& aabb = model.aabb;
    const auto cx = aabb.min[0] + (aabb.max[0] - aabb.min[0]) / 2;
    const auto cy = aabb.min[1] + (aabb.max[1] - aabb.min[1]) / 2;
    const auto cz = aabb.min[2] + (aabb.max[2] - aabb.min[2]) / 2;
    const auto scale = (!settings.normalize ? 1.0f : 
        1.0f / get_max_dimension(aabb));
    const auto sx = scale * settings.width;
    const auto sy = scale * settings.height;
    const auto sz = scale * settings.depth;
    const auto dx = (settings.center ? -cx : 0.0f) * sx;
    const auto dy = (settings.center ? -cy : 0.0f) * sy;
    const auto dz = (settings.center ? -cz : 0.0f) * sz;
    for (auto i = 0u; i < size; i += 8) {
      auto& px = vertices[i + 0];
      auto& py = vertices[i + 1];
      auto& pz = vertices[i + 2];
      auto& ny = vertices[i + 4];
      auto& nz = vertices[i + 5];
      auto& u = vertices[i + 6];
      auto& v = vertices[i + 7];
      px = px * sx + dx;
      py = py * sy + dy;
      pz = pz * sz + dz;
      u *= settings.scale_u;
      v *= settings.scale_v;
      if (settings.swap_yz) {
        std::swap(py, pz);
        std::swap(ny, nz);
      }
    }
  }
} // namespace

Model loadFile(const std::string& filename) noexcept {
  auto model = Model{};
  try {
    using namespace rapidobj;
    auto material = MaterialLibrary::SearchPath(".", Load::Optional);
    model.object = ParseFile(utf8_to_path(filename), material);
    Triangulate(model.object);
    model.aabb = compute_aabb(model);
  }
  catch (const std::exception& ex) {
    model.object.error = { 
      std::make_error_code(std::io_errc::stream),
      ex.what(),
      0
    };
  }
  return model;
}

std::string getError(const Model& model) {
  if (!model.object.error.code)
    return "";
  return model.object.error.code.message();
}

void setSettings(Model& model, const std::string& json) {
  model.settings = parse_settings(json);
}

std::vector<float> getVertices(const Model& model) {
  const auto& attributes = model.object.attributes;
  const auto& positions = attributes.positions;
  const auto& normals = attributes.normals;
  const auto& texcoords = attributes.texcoords;
  const auto count = positions.size() / 3;
  auto result = std::vector<float>();
  result.reserve(count * 8);
  for (auto i = 0u; i < count; ++i) {
    for (auto j = i * 3; j < (i + 1) * 3; ++j)
      result.push_back(positions[j]);
    for (auto j = i * 3; j < (i + 1) * 3; ++j)
      result.push_back(j < normals.size() ? normals[j] : 0.0f);
    for (auto j = i * 2; j < (i + 1) * 2; ++j)
      result.push_back(j < texcoords.size() ? texcoords[j] : 0.0f);
  }
  transform_vertices(result.data(), result.size(), model);
  return result;
}

size_t getShapeCount(const Model& model) {
  return model.object.shapes.size();
}

std::string getShapeName(const Model& model, size_t index) {
  if (index >= model.object.shapes.size())
    return { };
  return model.object.shapes[index].name;
}

std::vector<float> getShapeVertices(const Model& model, size_t index) {
  if (index >= model.object.shapes.size())
    return { };
  const auto& mesh = model.object.shapes[index].mesh;
  const auto& attributes = model.object.attributes;
  const auto positions = attributes.positions.data();
  const auto normals = attributes.normals.data();
  const auto texcoords = attributes.texcoords.data();

  auto result = std::vector<float>();
  result.reserve(mesh.indices.size() * 8);
  for (const auto& indices : mesh.indices) {
    for (auto i = 0; i < 3; ++i)
      result.push_back(positions[indices.position_index * 3 + i]);
    for (auto i = 0; i < 3; ++i)
      result.push_back((indices.normal_index >= 0 ? 
        (normals[indices.normal_index * 3 + i]) : 0));
    for (auto i = 0; i < 2; ++i)
      result.push_back((indices.texcoord_index >= 0 ? 
        texcoords[indices.texcoord_index * 2 + i] : 0));
  }
  transform_vertices(result.data(), result.size(), model);
  return result;
}

std::vector<int> getShapeIndices(const Model& model, size_t index) {
  if (index >= model.object.shapes.size())
    return { };
  const auto& mesh = model.object.shapes[index].mesh;
  auto result = std::vector<int>();
  result.reserve(mesh.indices.size());
  for (const auto& indices : mesh.indices)
    result.push_back(indices.position_index);
  return result;
}

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(loadFile)
DLLREFLECT_FUNC(getError)
DLLREFLECT_FUNC(setSettings)
DLLREFLECT_FUNC(getVertices)
DLLREFLECT_FUNC(getShapeCount)
DLLREFLECT_FUNC(getShapeName)
DLLREFLECT_FUNC(getShapeVertices)
DLLREFLECT_FUNC(getShapeIndices)
DLLREFLECT_END()
