instanceCount = 1
animateInstances = false

const library = app.loadLibrary("GenerateMesh")
const geometry = library.generate(JSON.stringify({
  type: "Trefoil Knot", slices: 32, stacks: 96, radius: 1.5,
  scaleU: 25, indexed: true
}))
const vertices = library.getVertices(geometry)
const indices = library.getIndices(geometry)
if (!vertices.length || !indices.length)
  throw new Error("Generating geometry failed")
vertexCount = vertices.length / 8
const vertexBlock = app.getItem("Vertices/Vertex")
const indexBlock = app.getItem("Indices/Triangle")
vertexBlock.rowCount = vertexCount
indexBlock.rowCount = indices.length / 3
app.setBlockData(vertexBlock, vertices)
app.setBlockData(indexBlock, indices)
