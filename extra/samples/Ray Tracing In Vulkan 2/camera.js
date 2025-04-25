
app.loadLibrary("gl-matrix.js")
const vec3 = glMatrix.vec3
const mat4 = glMatrix.mat4

const fovy = 20 * 3.14 / 180
const aspect = 1280 / 720
const eye = vec3.fromValues(13, 2, 3)
const center = vec3.fromValues(0, 0, 0)
const up = vec3.fromValues(0, 1, 0)

// reset sample count when camera moved
const radius = 5
const angle = app.mouse.coord[0] * 3
if (this.prevAngle == undefined || angle != prevAngle)
  totalSamples = 0
prevAngle = angle
eye[0] = Math.cos(angle) * radius
eye[2] = Math.sin(angle) * radius
center[1] = app.mouse.coord[1] * -1

view = mat4.lookAt(mat4.create(), eye, center, up)
projection = mat4.perspective(mat4.create(), fovy, aspect, 0.1, 10000.0)

viewInverse = mat4.invert(mat4.create(), view)
projectionInverse = mat4.invert(mat4.create(), projection)

samples = 8
totalSamples += samples
