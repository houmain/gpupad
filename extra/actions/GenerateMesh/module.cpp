
#include "dllreflect.h"
#include "jsonc.c"
#include <memory>
#include <cmath>

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4244) // conversion from 'double' to 'float'
# pragma warning(disable: 4305) // truncation from 'double' to 'float'
# pragma warning(disable: 4456) // declaration of 'a' hides previous local declaration
# pragma warning(disable: 4018) // '<': signed/unsigned mismatch
#endif

#if defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#define PAR_SHAPES_T uint32_t
#define PAR_SHAPES_IMPLEMENTATION
#include "par_shapes.h"
#if defined(_MSC_VER)
# pragma warning(pop)
#endif

namespace {
  // see: https://prideout.net/shapes
  enum class ShapeType {
    cube,
    cylinder,
    cone,
    torus,
    parametric_sphere,
    parametric_disk,
    subdivided_sphere,
    klein_bottle,
    trefoil_knot,
    hemisphere,
    plane,
    icosahedron,
    dodecahedron,
    octahedron,
    tetrahedron,
    rock,
    COUNT
  };

  struct Settings {
    ShapeType type{ ShapeType::cube };
    float width{ 2 };
    float height{ 2 };
    float depth{ 2 };
    float radius{ 1.0f };
    float scale_u{ 1.0f };
    float scale_v{ 1.0f };
    int slices{ 10 };
    int stacks{ 10 };
    int subdivisions{ 2 };
    int seed{ };
    bool facetted{ };
    bool inside_out{ };
    bool swap_yz{ };
  };

  struct FreeMesh {
    void operator()(par_shapes_mesh* mesh) const { par_shapes_free_mesh(mesh); }
  };
  using MeshPtr = std::unique_ptr<par_shapes_mesh, FreeMesh>;

  struct Geometry {
    Settings settings;
    std::unique_ptr<par_shapes_mesh, FreeMesh> mesh;
  };

  struct vec2 { float x, y; };
  struct vec3 { float x, y, z; };
  struct vec4 { float x, y, z, w; };

  struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 tex_coords;
  };

  const char* get_ShapeType_name(int index) {
    switch (static_cast<ShapeType>(index)) {
    case ShapeType::cube: return "Cube";
    case ShapeType::cylinder: return "Cylinder";
    case ShapeType::cone: return "Cone";
    case ShapeType::torus: return "Torus";
    case ShapeType::parametric_sphere: return "Parametric Sphere";
    case ShapeType::parametric_disk: return "Parametric Disk";
    case ShapeType::subdivided_sphere: return "Subdivided Sphere";
    case ShapeType::klein_bottle: return "Klein Bottle";
    case ShapeType::trefoil_knot: return "Trefoil Knot";
    case ShapeType::hemisphere: return "Hemisphere";
    case ShapeType::plane: return "Plane";
    case ShapeType::icosahedron: return "Icosahedron";
    case ShapeType::dodecahedron: return "Dodecahedron";
    case ShapeType::octahedron: return "Octahedron";
    case ShapeType::tetrahedron: return "Tetrahedron";
    case ShapeType::rock: return "Rock";
    case ShapeType::COUNT: break;
    }
    return "";
  }

  void parse_json(ShapeType& value, const std::string& json, const char* name) {
    if (auto string = json_get(json.c_str(), name)) {
      for (auto i = 0; i < static_cast<int>(ShapeType::COUNT); ++i)
        if (!std::strcmp(get_ShapeType_name(i), string)) {
          value = static_cast<ShapeType>(i);
          break;
        }
      free(string);
    }
  }

  void parse_json(int& value, const std::string& json, const char* name) {
    if (auto string = json_get(json.c_str(), name)) {
      value = std::atoi(string);
      free(string);
    }
  }

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
    parse_json(s.type, json, "type");
    parse_json(s.width, json, "width");
    parse_json(s.height, json, "height");
    parse_json(s.depth, json, "depth");
    parse_json(s.radius, json, "radius");
    parse_json(s.scale_u, json, "scaleU");
    parse_json(s.scale_v, json, "scaleV");
    parse_json(s.slices, json, "slices");
    parse_json(s.stacks, json, "stacks");
    parse_json(s.subdivisions, json, "subdivisions");
    parse_json(s.seed, json, "seed");
    parse_json(s.facetted, json, "facetted");
    parse_json(s.inside_out, json, "insideOut");
    parse_json(s.swap_yz, json, "swapYZ");
    return s;
  }

  // http://www.mvps.org/directx/articles/spheremap.htm
  void generate_tcoords_sphere(par_shapes_mesh* m) {
    PAR_FREE(m->tcoords);
    m->tcoords = PAR_CALLOC(float, m->npoints * 2);
    const float pi = std::atan(1.0f) * 4.0f;
    float normal[3];
    float* point = m->points;
    float* tcoord = m->tcoords;
    for (int i = 0; i < m->npoints; i++, point += 3, tcoord += 2) {
      par_shapes__copy3(normal, point);
      par_shapes__normalize3(normal);
      tcoord[0] = std::asin(normal[0]) / pi + 0.5f;
      tcoord[1] = std::asin(normal[2]) / pi + 0.5f;
    }
  }

  bool get_swap_yz(const Settings& s) {
    switch (s.type) {
    case ShapeType::hemisphere:
    case ShapeType::rock:
      return !s.swap_yz;
    default:
      return s.swap_yz;
    }
  }
} // namespace

int getTypeCount() {
  return static_cast<int>(ShapeType::COUNT);
}

const char* getTypeName(int index) {
  return get_ShapeType_name(index);
}

bool hasSlicesStacks(int type_index) {
  switch (static_cast<ShapeType>(type_index)) {
  case ShapeType::subdivided_sphere:
  case ShapeType::icosahedron:
  case ShapeType::dodecahedron:
  case ShapeType::octahedron:
  case ShapeType::tetrahedron:
  case ShapeType::cube:
  case ShapeType::rock:
    return false;
  default:
    return true;
  }
}

bool hasRadius(int type_index) {
  switch (static_cast<ShapeType>(type_index)) {
  case ShapeType::torus:
  case ShapeType::trefoil_knot:
    return true;
  default:
    return false;
  }
}

bool hasSubdivisions(int type_index) {
  switch (static_cast<ShapeType>(type_index)) {
  case ShapeType::subdivided_sphere:
  case ShapeType::rock:
    return true;
  default:
    return false;
  }
}

bool hasSeed(int type_index) {
  switch (static_cast<ShapeType>(type_index)) {
  case ShapeType::rock:
    return true;
  default:
    return false;
  }
}

Geometry generate(const std::string& json) {
  auto geometry = Geometry{ };
  auto& m = geometry.mesh;
  auto& s = geometry.settings;
  s = parse_settings(json);

  switch (s.type) {
  case ShapeType::COUNT:
    break;
  case ShapeType::cylinder: {
    m.reset(par_shapes_create_cylinder(s.slices, s.stacks));
    if (m) {
      auto top = par_shapes_create_parametric_disk(s.slices, s.stacks);
      auto bottom = par_shapes_create_parametric_disk(s.slices, s.stacks);
      par_shapes_translate(top, 0, 0, 1);
      par_shapes_scale(bottom, 1, 1, -1);
      par_shapes_invert(bottom, 0, 0);
      par_shapes_merge_and_free(m.get(), top);
      par_shapes_merge_and_free(m.get(), bottom);
      par_shapes_scale(m.get(), 1, 1, 2);
      par_shapes_translate(m.get(), 0, 0, -1);
    }
    break;
  }
  case ShapeType::cone: {
    m.reset(par_shapes_create_cone(s.slices, s.stacks));
    if (m) {
      auto bottom = par_shapes_create_parametric_disk(s.slices, s.stacks);
      par_shapes_scale(bottom, 1, 1, -1);
      par_shapes_invert(bottom, 0, 0);
      par_shapes_merge_and_free(m.get(), bottom);
      par_shapes_scale(m.get(), 1, 1, 2);
      par_shapes_translate(m.get(), 0, 0, -1);
    }
    break;
  }
  case ShapeType::torus:
    m.reset(par_shapes_create_torus(s.slices, s.stacks, s.radius));
    break;
  case ShapeType::parametric_sphere:
    m.reset(par_shapes_create_parametric_sphere(s.slices, s.stacks));
    break;
  case ShapeType::parametric_disk:
    m.reset(par_shapes_create_parametric_disk(s.slices, s.stacks));
    break;
  case ShapeType::subdivided_sphere:
    m.reset(par_shapes_create_subdivided_sphere(s.subdivisions));
    break;
  case ShapeType::klein_bottle:
    m.reset(par_shapes_create_klein_bottle(s.slices, s.stacks));
    if (m) {
      par_shapes_scale(m.get(), 0.125f, 0.125f, 0.125f);
    }
    break;
  case ShapeType::trefoil_knot:
    m.reset(par_shapes_create_trefoil_knot(s.slices, s.stacks, s.radius));
    break;
  case ShapeType::hemisphere:
    m.reset(par_shapes_create_hemisphere(s.slices, s.stacks));
    break;
  case ShapeType::plane:
    m.reset(par_shapes_create_plane(s.slices, s.stacks));
    if (m) {
      par_shapes_scale(m.get(), 2, 2, 2);
      par_shapes_translate(m.get(), -1, -1, 0);
    }
    break;
  case ShapeType::icosahedron:
    m.reset(par_shapes_create_icosahedron());
    break;
  case ShapeType::dodecahedron:
    m.reset(par_shapes_create_dodecahedron());
    break;
  case ShapeType::octahedron:
    m.reset(par_shapes_create_octahedron());
    break;
  case ShapeType::tetrahedron:
    m.reset(par_shapes_create_tetrahedron());
    break;
  case ShapeType::cube:
    m.reset(par_shapes_create_cube());
    if (m) {
      par_shapes_scale(m.get(), 2, 2, 2);
      par_shapes_translate(m.get(), -1, -1, -1);
    }
    break;
  case ShapeType::rock:
    m.reset(par_shapes_create_rock(s.seed, s.subdivisions));
    break;
  }

  if (m) {
    if (s.facetted)
      par_shapes_unweld(m.get(), true);

    if (s.facetted || !m->normals)
      par_shapes_compute_normals(m.get());

    if (s.facetted || !m->tcoords)
      generate_tcoords_sphere(m.get());
  }
  return geometry;
}

std::vector<float> getVertices(const Geometry& geometry) {
  if (!geometry.mesh)
    return { };
  const auto& m = *geometry.mesh;
  const auto& s = geometry.settings;

  const auto swap_yz = get_swap_yz(s);
  const auto scale_x = (s.width) / 2;
  const auto scale_y = (s.swap_yz ? s.height : s.depth) / 2;
  const auto scale_z = (s.swap_yz ? s.depth : s.height) / 2;
  const auto scale_normal = (s.inside_out ? -1.0f : 1.0f);
  const auto translate_u(s.scale_u < 0 ? 1.0f : 0.0f);
  const auto translate_v(s.scale_v < 0 ? 1.0f : 0.0f);

  auto vertices = std::vector<float>();
  vertices.reserve(8 * m.npoints);

  auto position = m.points;
  auto normal = m.normals;
  auto texcoords = m.tcoords;

  for (auto i = 0; i < m.npoints; ++i, position += 3, normal += 3, texcoords += 2) {
    vertices.push_back(position[0] * scale_x);
    vertices.push_back(position[swap_yz ? 2 : 1] * scale_y);
    vertices.push_back(position[swap_yz ? 1 : 2] * scale_z);

    vertices.push_back(normal[0] * scale_normal);
    vertices.push_back(normal[swap_yz ? 2 : 1] * scale_normal);
    vertices.push_back(normal[swap_yz ? 1 : 2] * scale_normal);

    vertices.push_back(texcoords[0] * s.scale_u + translate_u);
    vertices.push_back(texcoords[1] * s.scale_v + translate_v);
  }
  return vertices;
}

std::vector<uint32_t> getIndices(const Geometry& geometry) {
  if (!geometry.mesh)
    return { };
  const auto& m = *geometry.mesh;
  const auto& s = geometry.settings;
  const auto swap_yz = get_swap_yz(s);
  if (s.inside_out ^ swap_yz) {
    auto indices = std::vector<uint32_t>();
    indices.reserve(m.ntriangles * 3);
    auto triangle = m.triangles;
    for (auto i = 0; i < m.ntriangles; ++i, triangle += 3) {
      indices.push_back(triangle[0]);
      indices.push_back(triangle[2]);
      indices.push_back(triangle[1]);
    }
    return indices;
  }
  return { m.triangles, m.triangles + m.ntriangles * 3 };
}

std::vector<float> getVerticesUnweld(const Geometry& geometry) {
  const auto indices = getIndices(geometry);
  const auto vertices = getVertices(geometry);

  auto unweld = std::vector<float>();
  unweld.reserve(8 * indices.size());
  for (auto index : indices) {
    index *= 8;
    for (auto i = 0; i < 8; ++i)
      unweld.push_back(vertices[index++]);
  }
  return unweld;
}

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(getTypeCount)
DLLREFLECT_FUNC(getTypeName)
DLLREFLECT_FUNC(hasSlicesStacks)
DLLREFLECT_FUNC(hasRadius)
DLLREFLECT_FUNC(hasSubdivisions)
DLLREFLECT_FUNC(hasSeed)
DLLREFLECT_FUNC(generate)
DLLREFLECT_FUNC(getVertices)
DLLREFLECT_FUNC(getIndices)
DLLREFLECT_FUNC(getVerticesUnweld)
DLLREFLECT_END()
