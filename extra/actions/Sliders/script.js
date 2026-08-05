"use strict"

const manifest = {
  name: "&Sliders"
}

function hasOnlyNumericValues(values) {
  return values.every((value) => {
    const text = String(value).trim()
    return text.length > 0 && isFinite(Number(text))
  })
}

class Script {
  constructor() {
    this.sliders = [];
  }

  initializeUi(ui) {
    this.ui = ui

    app.trackItems(
      (item) => {
        const valueCount = item.values?.length ?? 0
        return (item.type == 'Binding' && item.bindingType == 'Uniform'
          && valueCount >= 1 && valueCount <= 4
          && hasOnlyNumericValues(item.values))
      },
      (binding, change) => {
        if (change === 'added') {
          this.sliders[binding.id] = ui.addSlider(binding)
        }
        else if (change === 'modified') {
          let slider = this.sliders[binding.id]
          if (slider)
            ui.updateSlider(binding, slider)
        }
        else if (change === 'removed') {
          let slider = this.sliders[binding.id]
          if (slider)
            ui.removeSlider(slider)
          delete this.sliders[binding.id]
        }
      })
  }
}


this.script = new Script()

app.openEditor("ui.qml").title = manifest.name

