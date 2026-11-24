
let vertices = new Array(count)
for (let i = 0; i < vertices.length; ++i) {
  let x, y
  do {
    x = Math.random() - 0.5
    y = Math.random() - 0.5
  } while (x * x + y * y > 0.25)
  vertices[i] = [x + 0.5, y + 0.5]
}
let indices = Delaunay.triangulate(vertices)
// convert to single array
vertices = [].concat.apply([], vertices)

let Vertices = app.session.getItem('Buffer/Vertices')
Vertices.setData(vertices)

let Indices = app.session.getItem('Buffer/Indices')
Indices.offset = vertices.length * 4
Indices.setData(indices)

let Draw = app.session.getItem('Draw')
Draw.count = indices.length
