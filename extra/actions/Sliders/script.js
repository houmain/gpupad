"use strict"

const manifest = {
  name: "&Sliders"
}

class Script {
  constructor() {
    this.sliders = [];
  }

  initializeUi(ui) {
    this.ui = ui

    app.trackItems(
      (item) => {
        return (item.type == 'Binding' && item.bindingType == 'Uniform');
      },
      (binding, change) => {
        if (change === 'added') {
          this.sliders[binding.id] = ui.addSlider(binding)
        }
        else if (change === 'modified') {
          let slider = this.sliders[binding.id]
          ui.updateSlider(binding, slider);
        }
        else if (change === 'removed') {
          let slider = this.sliders[binding.id]
          ui.removeSlider(slider);
          delete this.sliders[binding.id]
        }        
      })
  }
}


this.script = new Script()

app.openEditor("ui.qml").title = manifest.name

