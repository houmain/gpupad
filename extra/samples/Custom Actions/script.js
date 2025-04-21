
const meshTypes = [
  "Cube",
  "Cylinder",
  "Icosahedron",
  "Dodecahedron",
  "Octahedron",
  "Tetrahedron",
]
const meshType = meshTypes[app.frameIndex % meshTypes.length]

group = app.callAction("GenerateMesh", {
  name = "Mesh",
  group = app.session.item("Mesh"),
  dynamic = true,
  type = meshType,
  facetted = true,
})
