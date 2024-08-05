
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
    rotateX(0.5),
    rotateY(time));
}

function normalMatrix(time) {
  let m = modelMatrix(time);
  return [
    m[0], m[1], m[2],
    m[4], m[5], m[6],
    m[8], m[9], m[10]
  ]
}
