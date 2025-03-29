
#include "dllreflect.h"
#include "rapidobj.hpp"
#include "jsonc.c"
#include <span>

namespace {
  struct Settings {
    float width;
    float height;
    float depth;
    float scale_u;
    float scale_v;
    bool swap_yz;
  };

  struct Model {
    Settings settings;
    rapidobj::Result object;
  };

  struct AABB {
    float min[3];
    float max[3];
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

  void transform_vertices(float* vertices, size_t size, const Model& model) {
  }

  /*
  void centerPositions(Model& model) {
    float min[3];
    float max[3];
    for (auto c = 0; c < 3; ++c) {
      min[c] = std::numeric_limits<float>::max();
      max[c] = std::numeric_limits<float>::lowest();
    }

    auto i = 0u;
    for (const auto& position : model.attributes.positions) {
      const auto c = i % 3;
      min[c] = std::min(min[c], position);
      max[c] = std::max(max[c], position);
      ++i;
    }

    float delta[3];
    for (auto c = 0; c < 3; ++c)
      delta[c] = (max[c] - min[c]) / 2.0f;

    i = 0;
    for (auto& position : model.attributes.positions) {
      const auto c = i % 3;
      position += delta[c];
      ++i;
    }
  }

  void normalizePositions(Model& model) {
    auto scale = 0.0f;
    for (const auto& position : model.attributes.positions)
      scale = std::max(scale, std::abs(position));

    scale = 2.0f / scale;
    for (auto& position : model.attributes.positions)
      position *= scale;
  }

  void scalePositions(Model& model, float width, float height, float depth) {
    float whd[3] = { width, height, depth };
    auto i = 0u;
    for (auto& position : model.attributes.positions) {
      const auto c = i % 3;
      position *= whd[c];
      ++i;
    }
  }

  void scaleUV(Model& model, float scaleU, float scaleV) {
    float uv[3] = { scaleU, scaleV };
    auto i = 0u;
    for (auto& texcoord : model.attributes.texcoords) {
      const auto c = i % 2;
      texcoord *= uv[c];
      ++i;
    }
  }

  void swapYZ(Model& model) {
    auto& positions = model.attributes.positions;
    for (auto i = 0u; i < positions.size(); i += 3)
      std::swap(positions[i + 1], positions[i + 2]);
  }*/

} // namespace

Model loadFile(const std::string& filename) noexcept try {
  using namespace rapidobj;
  auto material = MaterialLibrary::SearchPath(".", Load::Optional);
  auto object = ParseFile(utf8_to_path(filename), material);
  Triangulate(object);
  return { Settings{}, std::move(object) };
}
catch (const std::exception& ex) {
  auto model = Model{};
  model.object.error = { 
    std::make_error_code(std::io_errc::stream),
    ex.what(),
    0
  };
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
  for (auto i = 0u; i < mesh.indices.size(); i += 3) {
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
DLLREFLECT_FUNC(getShapeVertices)
DLLREFLECT_FUNC(getShapeIndices)
DLLREFLECT_END()
