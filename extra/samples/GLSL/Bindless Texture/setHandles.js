
let names = [
  "marble.png",
  "planks.jpg",
]

let handles = []
for (let name of names)
  handles.push(app.session.getItem(name).getDeviceHandle())

app.session.getItem("Buffer").setData(handles)
