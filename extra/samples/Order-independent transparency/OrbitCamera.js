
// based on https://www.mbsoftworks.sk/tutorials/opengl4/026-camera-pt3-orbit-camera/ (MIT license)
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
    return this.lookAt(eye, this.center, this.upVector);
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

  subtract(a, b) {
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  }

  normalize(v) {
    const len = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return [v[0] / len, v[1] / len, v[2] / len];
  }

  crossProduct(a, b) {
    return [
      a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0]
    ];
  }

  addScaledVector(v, scaleVec, scale) {
    v[0] += scaleVec[0] * scale;
    v[1] += scaleVec[1] * scale;
    v[2] += scaleVec[2] * scale;
  }

  lookAt(eye, center, up) {
    const zaxis = this.normalize(this.subtract(eye, center));
    const xaxis = this.normalize(this.crossProduct(up, zaxis));
    const yaxis = this.crossProduct(zaxis, xaxis);

    const ex = -this.dot(xaxis, eye);
    const ey = -this.dot(yaxis, eye);
    const ez = -this.dot(zaxis, eye);

    return [
      [xaxis[0], yaxis[0], zaxis[0], 0],
      [xaxis[1], yaxis[1], zaxis[1], 0],
      [xaxis[2], yaxis[2], zaxis[2], 0],
      [ex, ey, ez, 1]
    ];
  }

  dot(a, b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  }
}

const center = [0,0,0]
const up = [0, 1, 0]
const radius = 50
const minRadius = 3
const azimuthAngle = 0
const polarAngle = 0
const camera = new OrbitCamera(center, up, radius, minRadius, azimuthAngle, polarAngle)

function updateOrbitCamera(mouse) {
  if (mouse.buttons[0] == 1) {
    camera.rotateAzimuth(2 * mouse.delta[0])
    camera.rotatePolar(2 * -mouse.delta[1])
  }
  if (mouse.buttons[2] == 1) {
    camera.zoom(mouse.delta[1])
  }
  return camera.getViewMatrix();
}

function perspective(fovy, aspect, near, far) {
  var y = 1 / Math.tan(fovy / 2 * Math.PI / 180);
  var x = y / aspect;
  var c = (near + far) / (near - far);
  var d = (near * far) / (near - far) * 2;
  return [
    [x, 0, 0, 0],
    [0, y, 0, 0],
    [0, 0, c,-1],
    [0, 0, d, 0]
  ];
}