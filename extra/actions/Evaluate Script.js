
const manifest = {
  name: `&Evaluate Script '${app.getFileTitle(app.currentEditor?.fileName)}'`,
  applicable: (app.currentEditor?.type == "Script")
}

app.evaluateScript(app.currentEditor.fileName)
