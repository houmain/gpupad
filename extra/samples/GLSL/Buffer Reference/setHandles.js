
let handles = [
  app.session.getItem("colors.bin").getDeviceHandle()
]

app.session.getItem("BufferReferences").setData(handles)
