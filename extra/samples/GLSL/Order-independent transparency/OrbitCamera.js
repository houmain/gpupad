
// based on https://www.mbsoftworks.sk/tutorials/opengl4/026-camera-pt3-orbit-camera/ (MIT license)

app.loadLibrary("gl-matrix.js")
const vec3 = glMatrix.vec3
const mat4 = glMatrix.mat4

class OrbitCamera {
  constructor(center, upVector, radius, minRadius, azimuthAngle, polarAngle) {
    this.center = center;
    this.upVector = upVector;
    this.radius = radius;
    this.minRadius = minRadius;
    this.azimuthAngle = azimuthAngle;
    this.polarAngle = polarAngle;
  }

  rotateAzimuth(radians) {
    this.azimuthAngle += radians;
    const fullCircle = 2 * Math.PI;
    this.azimuthAngle = ((this.azimuthAngle % fullCircle) + fullCircle) % fullCircle;
  }

  rotatePolar(radians) {
    this.polarAngle += radians;
    const polarCap = Math.PI / 2 - 0.001;
    if (this.polarAngle > polarCap)
      this.polarAngle = polarCap;
    if (this.polarAngle < -polarCap)
      this.polarAngle = -polarCap;
  }

  zoom(distance) {
    this.radius += distance;
    if (this.radius < this.minRadius)
      this.radius = this.minRadius;
  }

  moveHorizontal(distance) {
    const position = this.getEye();
    const viewVector = this.getNormalizedViewVector();
    const strafeVector = this.crossProduct(viewVector, this.upVector);
    this.addScaledVector(this.center, strafeVector, distance);
  }

  moveVertical(distance) {
    this.addScaledVector(this.center, this.upVector, distance);
  }

  getViewMatrix() {
    const eye = this.getEye();
    return mat4.lookAt(mat4.create(), eye, this.center, this.upVector);
  }

  getEye() {
    const sineAzimuth = Math.sin(this.azimuthAngle);
    const cosineAzimuth = Math.cos(this.azimuthAngle);
    const sinePolar = Math.sin(this.polarAngle);
    const cosinePolar = Math.cos(this.polarAngle);
    const x = this.center[0] + this.radius * cosinePolar * cosineAzimuth;
    const y = this.center[1] + this.radius * sinePolar;
    const z = this.center[2] + this.radius * cosinePolar * sineAzimuth;
    return [x, y, z];
  }

  getViewPoint() {
    return this.center;
  }

  getUpVector() {
    return this.upVector;
  }

  getNormalizedViewVector() {
    const eye = this.getEye();
    return this.normalize(this.subtract(this.center, eye));
  }

  getAzimuthAngle() {
    return this.azimuthAngle;
  }

  getPolarAngle() {
    return this.polarAngle;
  }

  getRadius() {
    return this.radius;
  }
}

const center = [0,0,0]
const up = [0, 1, 0]
const radius = 50
const minRadius = 3
const azimuthAngle = 0.5
const polarAngle = 0.2
const camera = new OrbitCamera(center, up, radius, minRadius, azimuthAngle, polarAngle)

function updateOrbitCamera() {
  if (app.mouse.buttons[0] == 1) {
    camera.rotateAzimuth(2 * app.mouse.delta[0])
    camera.rotatePolar(2 * -app.mouse.delta[1])
  }
  if (app.mouse.buttons[2] == 1) {
    camera.zoom(app.mouse.delta[1])
  }
  return camera.getViewMatrix();
}

function perspective(fovy, aspect, near, far) {
  return mat4.perspective(mat4.create(), fovy, aspect, near, far)
}
