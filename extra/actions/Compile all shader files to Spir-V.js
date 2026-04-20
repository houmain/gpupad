
for (let shader of app.findItems(item => item.type == 'Shader')) {
  const result = app.processShader(shader, "spirvBinary")
  if (typeof(result) == 'string') {
    console.error(result)
  }
  else {
    app.writeBinaryFile(shader.fileName + '.spv', result)
  }
}
