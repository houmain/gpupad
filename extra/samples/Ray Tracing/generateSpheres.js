
function random(min, max) {
  return Math.random() * (max - min) + min
}

const count = 1024
let spheres = new Array(count)
let aabbs = new Array(count)

for (let i = 0; i < spheres.length; ++i) {
  const x = random(-50, 50)
  const y = random(-50, 50)
  const z = random(-50, 50)
  const rad = random(0, 3)
  const r = random(0, 1)
  const g = random(0, 1)
  const b = random(0, 1)
  spheres[i] = [x,y,z, rad, r,g,b,1]
  aabbs[i] = [x - rad, y - rad, z - rad, x + rad, y + rad, z + rad]
}

// convert to single array
spheres = [].concat.apply([], spheres)
aabbs = [].concat.apply([], aabbs)

app.session.setBufferData(app.session.item("Spheres"), spheres)
app.session.setBufferData(app.session.item("AABBs"), aabbs)
