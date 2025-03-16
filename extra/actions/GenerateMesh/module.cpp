
#include "dllreflect.h"
#include <memory>
#include <cmath>

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4244) // conversion from 'double' to 'float'
# pragma warning(disable: 4305) // truncation from 'double' to 'float'
# pragma warning(disable: 4456) // declaration of 'a' hides previous local declaration
#endif
#define PAR_SHAPES_IMPLEMENTATION
#include "par_shapes.h"
#if defined(_MSC_VER)
# pragma warning(pop)
#endif

namespace {
  // see: https://prideout.net/shapes
  enum class Type {
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
    Type type{ Type::cube };
    float width{ 2 };
    float height{ 2 };
    float depth{ 2 };
    bool facetted{ };
    int slices{ 1 };
    int stacks{ 1 };
    float radius{ 1.0f };
    int subdivisions{ 0 };
    int seed{ };
    bool inside_out{ };
    float scale_u{ 1.0f };
    float scale_v{ 1.0f };
  };

  struct FreeMesh {
    void operator()(par_shapes_mesh* mesh) const { par_shapes_free_mesh(mesh); }
  };
  using MeshPtr = std::unique_ptr<par_shapes_mesh, FreeMesh>;

  struct vec2 { float x, y; };
  struct vec3 { float x, y, z; };
  struct vec4 { float x, y, z, w; };

  struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 tex_coords;
  };

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
      tcoord[1] = std::asin(normal[1]) / pi + 0.5f;
    }
  }
} // namespace

int getTypeCount() { return static_cast<int>(Type::COUNT); }

const char* getTypeName(int index) {
  switch (static_cast<Type>(index)) {
    case Type::cube: return "Cube";
    case Type::cylinder: return "Cylinder";
    case Type::cone: return "Cone";
    case Type::torus: return "Torus";
    case Type::parametric_sphere: return "Parametric Sphere";
    case Type::parametric_disk: return "Parametric Disk";
    case Type::subdivided_sphere: return "Subdivided Sphere";
    case Type::klein_bottle: return "Klein Bottle";
    case Type::trefoil_knot: return "Trefoil Knot";
    case Type::hemisphere: return "Hemisphere";
    case Type::plane: return "Plane";
    case Type::icosahedron: return "Icosahedron";
    case Type::dodecahedron: return "Dodecahedron";
    case Type::octahedron: return "Octahedron";
    case Type::tetrahedron: return "Tetrahedron";
    case Type::rock: return "Rock";
    case Type::COUNT: break;
  }
  return "";
}

Settings getSettings() { return { }; }
void setType(Settings& s, int index) { s.type = static_cast<Type>(index); }
void setWidth(Settings& s, float value) { s.width = value; }
void setHeight(Settings& s, float value) { s.height = value; }
void setDepth(Settings& s, float value) { s.depth = value; }
void setFacetted(Settings& s, bool value) { s.facetted = value; }
void setSlices(Settings& s, int value) { s.slices = value; }
void setStacks(Settings& s, int value) { s.stacks = value; }
void setRadius(Settings& s, float value) { s.radius = value; }
void setSubdivisions(Settings& s, int value) { s.subdivisions = value; }
void setSeed(Settings& s, int value) { s.seed = value; }
void setInsideOut(Settings& s, bool value) { s.inside_out = value; }
void setScaleU(Settings& s, float value) { s.scale_u = value; }
void setScaleV(Settings& s, float value) { s.scale_v = value; }

bool hasSlicesStacks(const Settings& s) { 
  switch (s.type) {
    case Type::subdivided_sphere:
    case Type::icosahedron:
    case Type::dodecahedron:
    case Type::octahedron:
    case Type::tetrahedron:
    case Type::cube:
    case Type::rock:
      return false;
    default:
      return true;
  }
}

bool hasRadius(const Settings& s) { 
  switch (s.type) {
    case Type::torus:
    case Type::trefoil_knot:
      return true;
    default: 
      return false;
  }
}

bool hasSubdivisions(const Settings& s) { 
  switch (s.type) {
    case Type::subdivided_sphere:
    case Type::rock:
      return true;
    default:
      return false;
  }
}

bool hasSeed(const Settings& s) { 
  switch (s.type) {
    case Type::rock:
      return true;
    default: 
      return false;
  }
}

MeshPtr generate(const Settings& s) {
  auto mesh = MeshPtr();

  switch (s.type) {
    case Type::COUNT:
      break;
    case Type::cylinder: {
      mesh.reset(par_shapes_create_cylinder(s.slices, s.stacks));
      if (mesh) {
        auto center = vec3{ 0, 0, 1 };
        auto normal = vec3{ 0, 0, 1 };
        auto top = par_shapes_create_disk(1.0f, s.slices,
          &center.x, &normal.x);
        par_shapes_merge_and_free(mesh.get(), top);
        center = vec3{ 0, 0, 0 };
        normal = vec3{ 0, 0, -1 };
        auto bottom = par_shapes_create_disk(1.0f, s.slices,
          &center.x, &normal.x);
        par_shapes_merge_and_free(mesh.get(), bottom);
      }
      break;
    }
    case Type::cone: {
      mesh.reset(par_shapes_create_cone(s.slices, s.stacks));
      if (mesh) {
        const auto center = vec3{ 0, 0, 0 };
        const auto normal = vec3{ 0, 0, -1 };
        auto bottom = par_shapes_create_disk(1.0f, s.slices,
          &center.x, &normal.x);
        par_shapes_merge_and_free(mesh.get(), bottom);
      }
      break;
    }
    case Type::torus:
      mesh.reset(par_shapes_create_torus(s.slices, s.stacks, s.radius));
      break;
    case Type::parametric_sphere:
      mesh.reset(par_shapes_create_parametric_sphere(s.slices, s.stacks));
      break;
    case Type::parametric_disk:
      mesh.reset(par_shapes_create_parametric_disk(s.slices, s.stacks));
      break;
    case Type::subdivided_sphere:
      mesh.reset(par_shapes_create_subdivided_sphere(s.subdivisions));
      break;
    case Type::klein_bottle:
      mesh.reset(par_shapes_create_klein_bottle(s.slices, s.stacks));
      break;
    case Type::trefoil_knot:
      mesh.reset(par_shapes_create_trefoil_knot(s.slices, s.stacks, s.radius));
      break;
    case Type::hemisphere:
      mesh.reset(par_shapes_create_hemisphere(s.slices, s.stacks));
      break;
    case Type::plane:
      mesh.reset(par_shapes_create_plane(s.slices, s.stacks));
      par_shapes_scale(mesh.get(), 2, 2, 2);
      par_shapes_translate(mesh.get(), -1, -1, 0);
      break;
    case Type::icosahedron:
      mesh.reset(par_shapes_create_icosahedron());
      break;
    case Type::dodecahedron:
      mesh.reset(par_shapes_create_dodecahedron());
      break;
    case Type::octahedron:
      mesh.reset(par_shapes_create_octahedron());
      break;
    case Type::tetrahedron:
      mesh.reset(par_shapes_create_tetrahedron());
      break;
    case Type::cube:
      mesh.reset(par_shapes_create_cube());
      par_shapes_scale(mesh.get(), 2, 2, 2);
      par_shapes_translate(mesh.get(), -1, -1, -1);
      break;
    case Type::rock:
      mesh.reset(par_shapes_create_rock(s.seed, s.subdivisions));
      break;
  }

  if (mesh) {
    if (s.width != 2 || s.height != 2 || s.depth != 2)
      par_shapes_scale(mesh.get(), s.width / 2, s.height / 2,
        s.depth / 2);

    if (s.facetted)
      par_shapes_unweld(mesh.get(), true);

    if (s.facetted || !mesh->normals)
      par_shapes_compute_normals(mesh.get());

    if (!mesh->tcoords)
      generate_tcoords_sphere(mesh.get());
  }
  return mesh;
}

std::vector<float> getVertices(const Settings& s, const MeshPtr& mesh) {
  if (!mesh)
    return { };

  const auto scale_normal = (s.inside_out ? -1.0f : 1.0f);
  const auto translate_u (s.scale_u < 0 ? 1.0f : 0.0f);
  const auto translate_v (s.scale_v < 0 ? 1.0f : 0.0f);

  auto vertices = std::vector<float>();
  vertices.reserve(8 * mesh->npoints);

  auto position = mesh->points;
  auto normal = mesh->normals;
  auto texcoords = mesh->tcoords;
  for (auto i = 0; i < mesh->npoints; ++i) {
    for (auto j = 0; j < 3; ++j)
      vertices.push_back(*position++);

    for (auto j = 0; j < 3; ++j)
      vertices.push_back(*normal++ * scale_normal);

    vertices.push_back(*texcoords++ * s.scale_u + translate_u);
    vertices.push_back(*texcoords++ * s.scale_v + translate_v);
  }
  return vertices;
}

std::vector<uint16_t> getIndices(const Settings& s, const MeshPtr& mesh) {
  if (!mesh)
    return { };

  if (s.inside_out) {
    auto indices = std::vector<uint16_t>();
    indices.reserve(mesh->ntriangles * 3);
    auto triangle = mesh->triangles;
    for (auto i = 0; i < mesh->ntriangles; ++i, ++triangle) {
      indices.push_back(triangle[0]);
      indices.push_back(triangle[2]);
      indices.push_back(triangle[1]);
    }
  }
  return { mesh->triangles, mesh->triangles + mesh->ntriangles * 3 };
}

DLLREFLECT_BEGIN()
DLLREFLECT_FUNC(getTypeCount)
DLLREFLECT_FUNC(getTypeName)
DLLREFLECT_FUNC(getSettings)
DLLREFLECT_FUNC(setType)
DLLREFLECT_FUNC(setWidth)
DLLREFLECT_FUNC(setHeight)
DLLREFLECT_FUNC(setDepth)
DLLREFLECT_FUNC(setFacetted)
DLLREFLECT_FUNC(setSlices)
DLLREFLECT_FUNC(setStacks)
DLLREFLECT_FUNC(setRadius)
DLLREFLECT_FUNC(setSubdivisions)
DLLREFLECT_FUNC(setSeed)
DLLREFLECT_FUNC(setInsideOut)
DLLREFLECT_FUNC(setScaleU)
DLLREFLECT_FUNC(setScaleV)
DLLREFLECT_FUNC(hasSlicesStacks)
DLLREFLECT_FUNC(hasRadius)
DLLREFLECT_FUNC(hasSubdivisions)
DLLREFLECT_FUNC(hasSeed)
DLLREFLECT_FUNC(generate)
DLLREFLECT_FUNC(getVertices)
DLLREFLECT_FUNC(getIndices)
DLLREFLECT_END()
