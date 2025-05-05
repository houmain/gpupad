
#include "dllreflect.h"
#include "rapidobj.hpp"
#include "jsonc.c"
#include <span>
#include <unordered_map>

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

  struct Vec3 {
    float x, y ,z;

    friend bool operator==(const Vec3& a, const Vec3& b) = default;
  };

  struct Vec2 {
    float x, y;

    friend bool operator==(const Vec2& a, const Vec2& b) = default;
  };

  struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;

    friend bool operator==(const Vertex& a, const Vertex& b) = default;
  };

  struct VertexHasher {
    size_t operator()(const Vertex& vertex) const {
      const auto hasher = std::hash<float>{ };
      return 
        (hasher(vertex.position.x) * 1) ^
        (hasher(vertex.position.y) * 2) ^
        (hasher(vertex.position.z) * 3) ^
        (hasher(vertex.normal.x) * 4) ^
        (hasher(vertex.normal.y) * 5) ^
        (hasher(vertex.normal.z) * 6) ^
        (hasher(vertex.texcoord.x) * 7) ^
        (hasher(vertex.texcoord.y) * 8);
    }
  };

  using Index = uint32_t;
  using ShapeIndices = std::vector<Index>;
  using VertexSet = std::unordered_map<Vertex, Index, VertexHasher>;

  struct Model {
    Settings settings;
    rapidobj::Result object;
    AABB aabb;
    std::string error;
    std::vector<Vertex> vertices;
    std::vector<ShapeIndices> shape_indices;
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

  Vec3 normalize(const Vec3& v) {
    auto len = v.x * v.x + v.y * v.y + v.z * v.z;
    if (len > 0) {
      len = 1.0f / std::sqrt(len);
      return { v.x * len, v.y * len, v.z * len };
    }
    return v;
  }

  Vec3 sub(const Vec3& a, const Vec3& b) {
    return {
      a.x - b.x,
      a.y - b.y,
      a.z - b.z,
    };
  }

  Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
      a.y * b.z - b.y * a.z,
			a.z * b.x - b.z * a.x,
			a.x * b.y - b.x * a.y
    };
  }

  Vec3 generate_face_normal(const std::span<const Vertex, 3> face_vertices) {
    return normalize(cross(
      sub(face_vertices[1].position, face_vertices[0].position),
      sub(face_vertices[2].position, face_vertices[0].position)));
  }

  std::array<Vertex, 3> get_face_vertices(std::span<const rapidobj::Index> face_indices,
      const rapidobj::Attributes& attributes) {
    auto face_vertices = std::array<Vertex, 3>{};
    for (auto i = 0; i < 3; ++i) {
      auto position = &attributes.positions[face_indices[i].position_index * 3];
      face_vertices[i].position.x = *position++;
      face_vertices[i].position.y = *position++;
      face_vertices[i].position.z = *position++;
    }
    const auto has_normals = std::all_of(face_indices.begin(), face_indices.end(), 
      [](const rapidobj::Index& index) { return index.normal_index >= 0; });
    if (has_normals) {
      for (auto i = 0; i < 3; ++i) {
        auto normal = &attributes.normals[face_indices[i].normal_index * 3];
        face_vertices[i].normal.x = *normal++;
        face_vertices[i].normal.y = *normal++;
        face_vertices[i].normal.z = *normal++;
      }
    }
    else {
      const auto normal = generate_face_normal(face_vertices);
      for (auto& face_vertex : face_vertices)
        face_vertex.normal = normal;
    }
    const auto has_texcoords = std::all_of(face_indices.begin(), face_indices.end(), 
      [](const rapidobj::Index& index) { return index.texcoord_index >= 0; });
    if (has_texcoords) {
      for (auto i = 0; i < 3; ++i) {
        auto texcoord = &attributes.texcoords[face_indices[i].texcoord_index * 2];
        face_vertices[i].texcoord.x = *texcoord++;
        face_vertices[i].texcoord.y = *texcoord++;
      }
    }
    else {
      // TODO:
    }
    return face_vertices;
  }

  std::array<Index, 3> add_face_vertices_once(std::vector<Vertex>& vertices,
      VertexSet& added_vertices, const std::array<Vertex, 3>& face_vertices) {
    auto face_indices = std::array<Index, 3>();
    for (auto i = 0u; i < face_vertices.size(); ++i) {
      const auto& face_vertex = face_vertices[i];
      const auto new_index = static_cast<Index>(vertices.size());
      const auto [it, inserted] = added_vertices.emplace(face_vertex, new_index);
      if (inserted)
        vertices.push_back(face_vertex);
      face_indices[i] = it->second;
    }
    return face_indices;
  }
} // namespace

Model loadFile(const std::string& filename) noexcept {
  auto model = Model{};
  try {
    using namespace rapidobj;
    auto material = MaterialLibrary::SearchPath(".", Load::Optional);
    model.object = ParseFile(utf8_to_path(filename), material);
    Triangulate(model.object);

    auto added_vertices = VertexSet{ };
    for (const auto& shape : model.object.shapes) {
      auto& shape_indices = model.shape_indices.emplace_back();
      shape_indices.reserve(shape.mesh.indices.size());
      for (auto i = 0u; i < shape.mesh.indices.size(); i += 3) {
        const auto face_indices = add_face_vertices_once(
          model.vertices, added_vertices, 
          get_face_vertices({ &shape.mesh.indices[i], 3 }, model.object.attributes));
        shape_indices.insert(shape_indices.end(),
          face_indices.begin(), face_indices.end());
      }
    }

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
  if (model.vertices.empty())
    return { };
  auto result = std::vector<float>();
  result.resize(model.vertices.size() * 8);
  std::memcpy(result.data(), model.vertices.data(), result.size() * sizeof(float));
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
  if (index >= model.shape_indices.size())
    return { };

  const auto& indices = model.shape_indices[index];
  auto result = std::vector<float>();
  result.resize(indices.size() * 8);
  auto pos = result.data();
  for (auto index : indices) {
    std::memcpy(pos, &model.vertices[index], 8 * sizeof(float));
    pos += 8;
  }
  transform_vertices(result.data(), result.size(), model);
  return result;
}

std::vector<Index> getShapeIndices(const Model& model, size_t index) {
  if (index >= model.shape_indices.size())
    return { };

  if (!model.settings.swap_yz)
    return model.shape_indices[index];
  
  auto swapped = model.shape_indices[index];
  for (auto i = 0u; i < swapped.size(); i += 3)
    std::swap(swapped[i + 1], swapped[i + 2]);
  return swapped;
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
