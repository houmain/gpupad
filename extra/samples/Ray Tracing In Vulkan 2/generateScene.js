
const scene = app.session.findItem("Scene")

let mesh = app.callAction("GenerateMesh", {
  name: "Mesh",
  type: "Trefoil Knot",
  group: app.session.findItem("Scene/Mesh"),
  parent: scene,
  slices: 50,
  stacks: 180,
  radius: 1.5,
  scaleU: 25,
  indexed: true,
  drawCall: false,
})

const vertices = mesh.items[0].items[0]
const indices = mesh.items[0].items[1]

const as = {
  type: "AccelerationStructure",
  items: [
    {
      items: [
        {
          geometryType: "Triangles",
          vertexBufferBlockId: vertices.id,
          indexBufferBlockId: indices.id,
        }
      ]
    }
  ]
}

app.session.replaceItems(scene, [
  mesh,
  as,
])

app.session.findItem("Trace Rays").accelerationStructureId = as.id

function Lambertian(diffuse, textureId = -1) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, textureId, 0.0, 0.0, 0];
}

function Metallic(diffuse, fuzziness, textureId = -1) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, textureId, fuzziness, 0.0, 1];
}

function Dielectric(refractionIndex, textureId = -1) {
	return [0.7, 0.7, 1.0, 1, textureId, 0.0, refractionIndex, 2];
}

function Isotropic(diffuse, textureId = -1) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, textureId, 0.0, 0.0, 3];
}

function DiffuseLight(diffuse, textureId = -1) {
	return [diffuse[0], diffuse[1], diffuse[2], 1, textureId, 0.0, 0.0, 4];
}

let materials = [
  Lambertian([1.0, 1.0, 1.0], 0),
  Dielectric(1.5),
  Lambertian([0.4, 0.2, 0.1]),
  Metallic([0.7, 0.6, 0.5], 0.0),
];

Materials = app.session.findItem("Materials/Material")

materials = [].concat.apply([], materials)
app.session.setBufferData("Materials", materials)

app.session.findItem("Bindings/MaterialArray").blockId = Materials.id
app.session.findItem("Bindings/VertexArray").blockId = vertices.id
app.session.findItem("Bindings/IndexArray").blockId = indices.id
