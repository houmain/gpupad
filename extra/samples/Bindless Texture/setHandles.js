
let handles = []
for (let name of ["marble.png", "Planks.jpg"])
  handles.push(app.session.getTextureHandle(name))

app.session.item("Buffer/Textures").rowCount = handles.length
app.session.setBufferData("Buffer", handles)
