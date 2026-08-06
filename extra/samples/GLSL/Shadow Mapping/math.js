
function rotateX(t) {
  var s = Math.sin(t);
  var c = Math.cos(t);
  return [
    1, 0, 0, 0,
    0, c,-s, 0,
    0, s, c, 0,
    0, 0, 0, 1
  ];
}

function rotateY(t) {
  var s = Math.sin(t);
  var c = Math.cos(t);
  return [
    c, 0,-s, 0,
    0, 1, 0, 0,
    s, 0, c, 0,
    0, 0, 0, 1
  ];
}

function rotateZ(t) {
  var s = Math.sin(t);
  var c = Math.cos(t);
  return [
    c,-s, 0, 0,
    s, c, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
  ];
}

function translation(x, y, z) {
  return [
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    x, y, z, 1
  ]; 
}

function scale(x, y, z) {
  y = y || x;
  z = z || x;
  return [
    x, 0, 0, 0,
    0, y, 0, 0,
    0, 0, z, 0,
    0, 0, 0, 1
  ]; 
}

function perspective(fovy, aspect, near, far) {
  var y = 1 / Math.tan(fovy / 2 * Math.PI / 180);
  var x = y / aspect;
  var c = (near + far) / (near - far);
  var d = (near * far) / (near - far) * 2;
  return [
    x, 0, 0, 0,
    0, y, 0, 0,
    0, 0, c,-1,
    0, 0, d, 0
  ];
}

function ortho(left, right, bottom, top, near, far) {
  var x = 2 / (right - left);
	var y = 2 / (top - bottom);
	var c = -2 / (far - near);
	var d = -(right + left) / (right - left);
	var e = -(top + bottom) / (top - bottom);
	var f = -(far + near) / (far - near);
  return [
    x, 0, 0, 0,
    0, y, 0, 0,
    0, 0, c, 0,
    d, e, f, 1
  ];	
}

function dot(a, b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

function cross(a, b) {
  return [
    a[1] * b[2] - b[1] * a[2],
  	a[2] * b[0] - b[2] * a[0],
  	a[0] * b[1] - b[0] * a[1]
  ];
}

function sub(a, b) {
  return [
    a[0] - b[0],
    a[1] - b[1],
    a[2] - b[2]
  ];
}

function normalize(v) {
  var d = 1.0 / Math.sqrt(dot(v, v));
  return [
    v[0] * d, 
    v[1] * d, 
    v[2] * d
  ];
}

function transpose(a) {
  var b = [];
  for (var i = 0; i < 4; i++)
    for (var j = 0; j < 4; j++)
      b[i * 4 + j] = a[j * 4 + i];
  return b;
}

function lookat(eye, center, up) {
  var f = normalize(sub(center, eye));
  var s = normalize(cross(f, up));
  var u = cross(s, f);
  return [
    s[0], u[0], -f[0], 0,
    s[1], u[1], -f[1], 0,
    s[2], u[2], -f[2], 0,
    -dot(s, eye), -dot(u, eye), dot(f, eye), 1,
  ];
}

function multiply(a, b) {
  var c = [];
  for (var i = 0; i < 4; i++)
    for (var j = 0; j < 4; j++) {
      var t = 0;
      for (var k = 0; k < 4; k++)
        t += a[i * 4 + k] * b[k * 4 + j];
      c[i * 4 + j] = t;
    }
  return c;  
}


function modelMatrix(time) {
  return multiply(      
    rotateX(-3.141516 / 2),
    rotateY(time));
}

function normalMatrix(time) {
  return modelMatrix(time)
}
