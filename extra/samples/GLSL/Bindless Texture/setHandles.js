
let names = [
  "marble.png",
  "planks.jpg",
]

let handles = []
for (let name of names)
  handles.push(app.getTextureHandle(name))

app.setBufferData("Buffer", handles)
