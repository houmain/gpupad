app.loadLibrary("gl-matrix.js")
const mat4 = glMatrix.mat4
const angle = 1.2 + app.mouse.coord[0] * 3
const eye = [Math.cos(angle) * 11, 3, Math.sin(angle) * 11]
const center = [0, -app.mouse.coord[1] * 2, 0]
const view = mat4.lookAt(mat4.create(), eye, center, [0, 1, 0])
const projection = mat4.perspective(mat4.create(), 35 * Math.PI / 180,
  1280 / 720, 0.1, 10000)
viewInverse = mat4.invert(mat4.create(), view)
projectionInverse = mat4.invert(mat4.create(), projection)

transforms = []
for (let i = 0; i < instanceCount; ++i) {
  const rotation = animateInstances ? Math.sin(app.time + i) * 0.6 : i * 0.25
  const c = Math.cos(rotation) * 0.8
  const s = Math.sin(rotation) * 0.8
  const x = (i - (instanceCount - 1) * 0.5) * 3.1
  transforms.push([c, 0, s, x, 0, 0.8, 0, 0, -s, 0, c, 0])
}
