"use strict"

const manifest = {
  name: "&Sliders"
}

class Script {
  constructor() {
    this.library = app.loadLibrary("GenerateMesh")
  }
  
  initializeUi(ui) {
    this.ui = ui
    
    const bindings = app.session.findItems((item) => {
      return (item.type == 'Binding');
    })
    
    let y = 10
    for (let binding of bindings) {
      //console.log(binding.name)
      ui.addSlider(binding, y)
      y += 30
    }
  }
}


this.script = new Script()

app.openEditor("ui.qml", manifest.name)

