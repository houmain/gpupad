"use strict"

var manifest = { name: "&DXR Showcases" }

this.script = {
  showcase() {
    return Number(app.session.getItem("Settings/Showcase").values[0])
  },

  selectShowcase(index) {
    const binding = app.session.getItem("Settings/Showcase")
    if (Number(binding.values[0]) === index)
      return
    binding.values = [String(index)]
    app.evaluate(true)
  }
}

{
  const sceneScript = app.session.getItem("Shared Scene").findItem(item => item.type === "Script"
    && item.fileName.endsWith("generateScene.js"), false).fileName
  const directory = sceneScript.slice(0, Math.max(sceneScript.lastIndexOf("/"),
    sceneScript.lastIndexOf("\\")) + 1)
  app.session.getItem("Output Image").openEditor()
  const panel = app.openEditor(directory + "actions/DxrShowcases/ui.qml")
  panel.title = "DXR Showcases"
  this.result = { type: panel.type, title: panel.title, fileName: panel.fileName }
  if (app.evaluation === "Paused")
    app.evaluate()
}
