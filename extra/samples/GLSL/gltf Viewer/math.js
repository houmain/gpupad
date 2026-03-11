

app.loadLibrary("gl-matrix.js")
const vec3 = glMatrix.vec3
const mat4 = glMatrix.mat4

function modelMatrix(time) {
  const m = mat4.create()
  mat4.fromXRotation(m, 3.141516 / 2)
  mat4.rotateZ(m, m, -time)
  return m
}

function normalMatrix(time) {
  return modelMatrix(time)
}

function viewProjectionMatrix(time) {
  const m = mat4.create()
  mat4.perspective(m, 40 * Math.PI / 180, 1.0, 0.1, 1000)
  mat4.translate(m, m, vec3.fromValues(0, 0, -3.5))
  return m
}
