
let names = [
  "marble.png",
  "planks.jpg",
]

let handles = []
for (let name of names)
  handles.push(app.session.getTextureHandle(name))

app.session.setBufferData("Buffer", handles)
