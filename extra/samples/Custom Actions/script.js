
// cycle through OBJ files in directory
if (!this.files)
  this.files = app.enumerateFiles("**/*.obj")

const meshTypes = [
  "Cylinder",
  "Icosahedron",
  "Dodecahedron",
  "Octahedron",
  "Tetrahedron",
]
  
const total = meshTypes.length + this.files.length
const index = app.frameIndex % total

if (index < this.files.length) {
  group = app.callAction("ImportOBJ", {
    fileName = this.files[index],
    name = "Mesh",
    group = app.session.item("Mesh"),
    indexed = true,
    dynamic = true,
    normalize = true,
    center = true,
  })
  console.log(`Wavefront OBJ (${index + 1}/${this.files.length}): '${this.files[index]}'`)
}
else {
  const meshType = meshTypes[index - this.files.length]
  group = app.callAction("GenerateMesh", {
    name = "Mesh",
    group = app.session.item("Mesh"),
    indexed = true,
    dynamic = true,
    type = meshType,
    facetted = true,
  })
  console.log(`Generated mesh: ${meshType}`)
}
