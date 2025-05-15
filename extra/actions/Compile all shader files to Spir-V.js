
for (let shader of app.session.findItems(item => item.type == 'Shader')) {
  const result = app.session.processShader(shader, "spirvBinary")
  if (typeof(result) == 'string') {
    console.error(result)
  }
  else {
    app.writeBinaryFile(shader.fileName + '.spv', result)
  }
}
