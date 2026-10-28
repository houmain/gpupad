// change only the mode number, save and reset evaluation with F5
// mode 0: static objects with shader-record colors
// mode 1: rotating instances with TLAS rebuild
// mode 2: same rotation with TLAS update
// mode 3: deforming meshes with BLAS rebuild
// mode 4: same deformation with BLAS update and dependent TLAS refresh
const mode = 4
instanceCount = 3
animateInstances = mode == 1 || mode == 2
vertexCount = 0

app.getItem("Trace Rays").checked = false
app.getItem("Deform/Deform Mesh").checked = false
if (app.getItem("Scene/Shared1").sourceInstanceId === undefined)
  throw new Error("This sample requires shared-BLAS instance controls (#57)")
if (app.getItem("Program").rayTracing === undefined)
  throw new Error("This sample requires explicit DXR records (#58)")
if (app.getItem("Scene").allowTlasUpdate === undefined)
  throw new Error("This sample requires TLAS lifecycle support (#59)")
if (app.getItem("Scene").allowBlasUpdate === undefined)
  throw new Error("This sample requires BLAS lifecycle support (#60)")

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

const restBlock = app.getItem("RestVertices/Vertex")
restBlock.rowCount = vertexCount
app.setBlockData(restBlock, vertices)
app.getItem("Scene").allowTlasUpdate = mode == 2 || mode == 4
app.getItem("Scene").allowBlasUpdate = mode == 4
app.getItem("Deform/Deform Mesh").checked = mode == 3 || mode == 4
app.getItem("Trace Rays").checked = true
