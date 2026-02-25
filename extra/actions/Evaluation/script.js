"use strict"

const manifest = {
  name: "&Evaluation"
}

class Script {
  constructor() {
  }
  
  initializeUi(ui) {
    this.ui = ui
  }
}

this.script = new Script()

app.openEditor("ui.qml", manifest.name)
