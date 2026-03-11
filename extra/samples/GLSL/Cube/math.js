
app.loadLibrary("gl-matrix.js")
const vec3 = glMatrix.vec3
const mat4 = glMatrix.mat4

function rotateY(t) {
  const m = mat4.create()
  return mat4.rotateY(m, m, t)
}

function perspective(fovy, aspect, near, far) {
  const m = mat4.create()
  return mat4.perspective(m, fovy * Math.PI / 180, aspect, near, far)
}
