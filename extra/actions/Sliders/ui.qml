pragma ComponentBehavior: Bound

import App

ThemedScrollView {
  id: root
  implicitWidth: sliders.implicitWidth + 40
  implicitHeight: 240
  contentHeight: sliders.y + sliders.implicitHeight + 20

  ColumnLayout {
    id: sliders
    spacing: 20
    x: 20
    y: 20
    width: Math.max(0, root.availableWidth - 40)

    Label {
      text: `Session: '${app.session.name}'`
    }

    Label {
      text: `Current Editor: '${app.currentEditor?.title}'`
    }

    Label {
      text: `Viewport Size: '${app.currentEditor?.viewportSize}'`
    }
  }

  Component.onCompleted: script.initializeUi(root)

  Component {
    id: sliderComponent

    UniformSlider {}
  }

  function bindingValues(binding) {
    const result = []
    const count = Math.min(4, binding.values.length)
    for (let i = 0; i < count; ++i) {
      const value = parseFloat(binding.values[i])
      result.push(isFinite(value) ? value : 0)
    }
    return result
  }

  function addSlider(binding) {
    const values = bindingValues(binding)
    const slider = sliderComponent.createObject(sliders, {
      bindingId: binding.id
    })

    let minimum = 0
    let maximum = 1
    for (const value of values) {
      minimum = Math.min(minimum, value)
      maximum = Math.max(maximum, value)
    }
    if (minimum === maximum)
      maximum = minimum + 1
    slider.minimum = minimum
    slider.maximum = maximum
    updateSlider(binding, slider)
    return slider
  }

  function updateSlider(binding, slider) {
    const values = bindingValues(binding)
    slider.name = binding.name
    slider.valueCount = values.length
    slider.values = values
  }

  function removeSlider(slider) {
    slider.destroy()
  }
}
